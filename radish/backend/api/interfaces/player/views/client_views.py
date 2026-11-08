"""
Endpunkte fuer den Spielclient unter api/client/.

Ein Client kann hierueber ein Spiel erstellen, einem Spiel beitreten, es
als Host starten, es wieder verlassen, die offenen Spiele auflisten, sein
eigenes Spiel abfragen und seinen eigenen Zustand abfragen. Alles ausser
Zustand ist echt umgesetzt; Zustand ist noch Platzhalter (siehe dessen
Docstring).

Ein Spiel beginnt in der Lobby (siehe models.GameStatus): erst wenn der
Host startet (GameStartView) und die Instanz das Spiel im selben Aufruf
aufgesetzt hat, laeuft es. Das Frontend fragt den Zustand per Polling ueber
CurrentGameView ab -- so erfaehrt auch der zweite Spieler vom Start.

Kommandos an den Spielserver und ein laufender Event-Stream dazu laufen
nicht mehr ueber eigene REST-Endpunkte (ehemals CommandView unter
client/command/ bzw. StreamView unter client/stream/), sondern ueber den
WebSocket (siehe api/interfaces/player/views/consumers.py, EchoConsumer).
"""

from typing import cast

# Nur fuer Type Hints: das Projekt nutzt Djangos Standard-User-Model (kein
# AUTH_USER_MODEL), get_user_model() liefert also genau diese Klasse.
# pylint: disable-next=imported-auth-user
from django.contrib.auth.models import AnonymousUser, User
from django.core.paginator import Page, Paginator
from django.db import transaction
from django.db.models import Q, QuerySet
from rest_framework import status
from rest_framework.permissions import IsAuthenticated
from rest_framework.request import Request
from rest_framework.response import Response
from rest_framework.views import APIView
from rest_framework_simplejwt.tokens import Token

from api.domain.army_queries import armies_with_costs
from api.domain.games import end_started_game
from api.domain.servers import access, instance_client
from api.interfaces.player.roles.has_player_role import HasPlayerRole
from api.interfaces.player.serializers.game_create_serializer import GameCreateSerializer
from api.interfaces.player.serializers.game_detail_serializer import GameDetailSerializer
from api.interfaces.player.serializers.game_join_serializer import GameJoinSerializer
from api.interfaces.player.serializers.game_list_serializer import GameListSerializer
from api.models import Army, Game, GameServer, GameStatus, Player


def _sync_player_name(player: Player, request: Request) -> None:
    """
    Uebernimmt den Keycloak-Benutzernamen ("preferred_username" im
    validierten Access-Token, siehe auth/roles/realm_roles._realm_roles fuer
    request.auth) in Player.name, falls er sich geaendert hat.
    """
    # DRF typisiert request.auth mit seinem eigenen authtoken-Token; hier
    # setzt es aber KeycloakJWTAuthentication, also ein simplejwt-Token
    # (verhaelt sich wie ein Mapping auf seine Klaims).
    token: Token | None = cast(Token | None, request.auth)
    name: str = (token.get("preferred_username") if token is not None else None) or ""
    if name and name != player.name:
        player.name = name
        player.save(update_fields=["name"])


def _get_or_create_player(request: Request) -> Player:
    """
    Loest den durch Keycloak authentifizierten Django-User (request.user,
    siehe KeycloakJWTAuthentication in api/interfaces/auth/authentication.py) zu seinem
    Player auf. Gibt es noch keinen, wird jetzt einer angelegt -- die
    zufaellige, oeffentlich sichtbare Kennung (Player.identifier) erzeugt
    Player.save() dabei weiterhin selbst.

    Anders als frueher hat der Client keinen Einfluss mehr darauf, welcher
    Player gemeint ist: das ergibt sich zwingend aus request.user, also aus
    dem validierten Access-Token -- ein player_identifier im Request-Body
    entfaellt komplett (siehe serializers/game_create_serializer.py).
    """
    player: Player
    player, _ = Player.objects.get_or_create(user=request.user)
    _sync_player_name(player, request)
    return player


def _army_for_game(
    player: Player, army_id: int, points_limit: int
) -> tuple[Army, None] | tuple[None, Response]:
    """
    Die Armee, mit der ein Spieler antreten will: sie muss ihm gehoeren,
    mindestens eine Einheit haben und darf hoechstens points_limit kosten.
    Liefert (army, None) oder (None, Fehler-Response).
    """
    army: Army | None = armies_with_costs().filter(pk=army_id, player=player).first()
    if army is None:
        return None, Response(
            {"detail": "Armee nicht gefunden."}, status=status.HTTP_400_BAD_REQUEST
        )
    if not army.units.all():
        return None, Response(
            {"detail": f"Die Armee „{army.name}“ hat noch keine Einheiten."},
            status=status.HTTP_400_BAD_REQUEST,
        )
    cost: int = army.total_cost()
    if cost > points_limit:
        return None, Response(
            {
                "detail": f"Die Armee „{army.name}“ kostet {cost} Punkte, "
                f"erlaubt sind hoechstens {points_limit}."
            },
            status=status.HTTP_400_BAD_REQUEST,
        )
    return army, None


class GameCreateView(APIView):
    """
    Erstellt ein neues Spiel.

    Voraussetzungen:
    - Der Ersteller (request.user, also der authentifizierte Nutzer) ist
      noch in keinem anderen Spiel Host oder Mitspieler.
    - Es gibt mindestens einen freien GameServer (is_occupied=False); der
      wird dem neuen Spiel fest zugewiesen und sofort als belegt markiert.
    - Die gewaehlte Armee passt zum Punktelimit des Spiels (siehe
      _army_for_game).

    select_for_update() beim Serversuchen + transaction.atomic(), damit
    zwei gleichzeitige Anfragen sich nicht denselben freien Server
    schnappen koennen.
    """

    permission_classes = [IsAuthenticated, HasPlayerRole]

    def post(self, request: Request) -> Response:
        serializer: GameCreateSerializer = GameCreateSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        name: str = serializer.validated_data["name"]
        password: str = serializer.validated_data["password"]
        points_limit: int = serializer.validated_data["points_limit"]

        player: Player = _get_or_create_player(request)

        already_in_game: bool = Game.objects.filter(
            Q(host=player) | Q(second_player=player)
        ).exists()
        if already_in_game:
            return Response(
                {"detail": "Spieler ist bereits in einem Spiel."},
                status=status.HTTP_409_CONFLICT,
            )

        army: Army | None
        error: Response | None
        army, error = _army_for_game(player, serializer.validated_data["army_id"], points_limit)
        if error is not None:
            return error

        with transaction.atomic():
            server: GameServer | None = (
                GameServer.objects.select_for_update()
                .filter(is_occupied=False)
                .first()
            )
            if server is None:
                return Response(
                    {"detail": "Keine freie Serverinstanz verfuegbar."},
                    status=status.HTTP_409_CONFLICT,
                )
            server.is_occupied = True
            server.save(update_fields=["is_occupied"])
            game: Game = Game.objects.create(
                name=name,
                password=password,
                server=server,
                host=player,
                points_limit=points_limit,
                host_army=army,
            )

        return Response(
            GameDetailSerializer(game, context={"player": player}).data,
            status=status.HTTP_201_CREATED,
        )


def _check_joinable(game: Game, password: str) -> Response | None:
    """
    Prueft, ob man dem (gesperrten) Spiel mit diesem Passwort beitreten
    kann. Liefert die Fehler-Response oder None.
    """
    if game.password != password:
        return Response(
            {"detail": "Falsches Passwort."},
            status=status.HTTP_403_FORBIDDEN,
        )

    if game.second_player_id or game.status != GameStatus.LOBBY:
        return Response(
            {"detail": "Spiel ist bereits voll."},
            status=status.HTTP_409_CONFLICT,
        )
    return None


class GameJoinView(APIView):
    """
    Laesst einen Spieler einem bestehenden Spiel als zweiten Mitspieler
    beitreten.

    Voraussetzungen:
    - Der beitretende Spieler (request.user) ist noch in keinem Spiel
      Host oder Mitspieler -- dieselbe Regel wie beim Erstellen.
    - Das Spiel (identifiziert ueber seinen Namen) muss existieren.
    - Das mitgeschickte Passwort muss zum Spiel passen.
    - Das Spiel darf noch keinen zweiten Spieler haben und muss in der
      Lobby sein.
    - Die gewaehlte Armee passt zum Punktelimit des Spiels (siehe
      _army_for_game).

    select_for_update() auf das gefundene Game + transaction.atomic(),
    damit nicht zwei Spieler gleichzeitig denselben freien zweiten Platz
    belegen koennen (dieselbe Ueberlegung wie bei der Serverzuteilung in
    GameCreateView).
    """

    permission_classes = [IsAuthenticated, HasPlayerRole]

    def post(self, request: Request) -> Response:
        serializer: GameJoinSerializer = GameJoinSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        name: str = serializer.validated_data["name"]
        password: str = serializer.validated_data["password"]

        player: Player = _get_or_create_player(request)

        already_in_game: bool = Game.objects.filter(
            Q(host=player) | Q(second_player=player)
        ).exists()
        if already_in_game:
            return Response(
                {"detail": "Spieler ist bereits in einem Spiel."},
                status=status.HTTP_409_CONFLICT,
            )

        game: Game
        army: Army | None
        error: Response | None
        with transaction.atomic():
            try:
                game = Game.objects.select_for_update().get(name=name)
            except Game.DoesNotExist:
                return Response(
                    {"detail": "Spiel nicht gefunden."},
                    status=status.HTTP_404_NOT_FOUND,
                )

            error = _check_joinable(game, password)
            if error is not None:
                return error

            army, error = _army_for_game(
                player, serializer.validated_data["army_id"], game.points_limit
            )
            if error is not None:
                return error

            game.second_player = player
            game.second_player_army = army
            game.save(update_fields=["second_player", "second_player_army"])

        return Response(
            GameDetailSerializer(game, context={"player": player}).data,
            status=status.HTTP_200_OK,
        )


def _check_startable(game: Game, player: Player) -> Response | None:
    """
    Prueft, ob "player" das (gesperrte) Spiel starten darf: er ist Host, das
    Spiel ist in der Lobby und hat einen zweiten Spieler. Liefert die
    Fehler-Response oder None.
    """
    if game.host_id != player.id:
        return Response(
            {"detail": "Nur der Host kann das Spiel starten."},
            status=status.HTTP_403_FORBIDDEN,
        )
    if game.status != GameStatus.LOBBY:
        return Response(
            {"detail": "Das Spiel wurde bereits gestartet."},
            status=status.HTTP_409_CONFLICT,
        )
    if not game.second_player_id:
        return Response(
            {"detail": "Es fehlt noch ein zweiter Spieler."},
            status=status.HTTP_409_CONFLICT,
        )
    return None


class GameStartView(APIView):
    """
    Laesst den Host sein Spiel aus der Lobby heraus starten.

    Voraussetzungen:
    - Der anfragende Spieler ist Host seines Spiels.
    - Das Spiel ist in der Lobby und hat einen zweiten Spieler.

    Synchron: die Instanz setzt das Spiel noch in diesem Aufruf auf (siehe
    instance_client.start_game). Klappt das, laeuft das Spiel (RUNNING);
    sonst bleibt es unveraendert in der Lobby (502).

    Die Zeile des Spiels bleibt dabei gesperrt (select_for_update), auch
    waehrend des Aufrufs an die Instanz: ein gleichzeitiges Verlassen oder
    ein zweiter Start wartet, bis der Start entschieden ist. Das Aufsetzen
    ist kurz (siehe instance_client.TIMEOUT_SECONDS), die Sperre also auch.
    """

    permission_classes = [IsAuthenticated, HasPlayerRole]

    def post(self, request: Request) -> Response:
        user: User | AnonymousUser = request.user
        with transaction.atomic():
            # Das Spiel direkt ueber den User statt erst den Player zu suchen;
            # ohne Player-Objekt findet die Abfrage kein Spiel.
            # of=("self",): nur die Zeile des Spiels sperren -- second_player
            # ist nullable, und durch einen Outer Join sperrt Postgres nicht.
            game: Game | None = (
                Game.objects.select_for_update(of=("self",))
                # Die Armeen fuer die Antwort (GameDetailSerializer, Armeenamen).
                .select_related("server", "host", "second_player", "host_army", "second_player_army")
                .filter(Q(host__user=user) | Q(second_player__user=user))
                .first()
            )
            if game is None:
                return Response(
                    {"detail": "Spieler ist in keinem Spiel."},
                    status=status.HTTP_409_CONFLICT,
                )
            # Der anfragende Spieler steckt schon im Spiel. Sein Name wird vor
            # dem Start nachgezogen: das Spielstart-Dokument liest ihn aus
            # game.host bzw. game.second_player -- demselben Objekt.
            player: Player | None = game.host if game.host.user_id == user.id else game.second_player
            # Gefunden ueber host__user oder second_player__user: ist der User
            # nicht Host, ist er second_player, und der ist dann nicht None.
            assert player is not None
            _sync_player_name(player, request)
            error: Response | None = _check_startable(game, player)
            if error is not None:
                return error

            # Neue Codes fuer jeden Start: die Instanz nimmt sie in ihre
            # Whitelist, der Consumer setzt sie vor die Nachrichten (access.py).
            # Gespeichert wird erst, wenn die Instanz das Spiel aufgesetzt hat.
            game.host_zucchini_code = access.new_zucchini_code()
            game.second_player_zucchini_code = access.new_zucchini_code()

            try:
                instance_client.start_game(game)
            except instance_client.InstanceError as exc:
                return Response(
                    {"detail": f"Spielinstanz konnte nicht gestartet werden: {exc}"},
                    status=status.HTTP_502_BAD_GATEWAY,
                )

            game.status = GameStatus.RUNNING
            game.save(update_fields=["status", "host_zucchini_code", "second_player_zucchini_code"])

        return Response(
            GameDetailSerializer(game, context={"player": player}).data,
            status=status.HTTP_200_OK,
        )


class GameListView(APIView):
    """
    Liefert die Spiele in der Lobby mit Name und Anzahl der bereits
    angemeldeten Spieler (1, solange nur der Ersteller da ist, sonst 2).
    Gestartete Spiele fehlen: denen kann ohnehin niemand mehr beitreten.

    Seitenweise, je PAGE_SIZE Spiele, aelteste zuerst (wer am laengsten
    wartet, steht oben). Die Seite kommt als "?page=" (ab 1); ungueltige
    Werte ergeben Seite 1, zu grosse die letzte -- so landet ein Client,
    dessen Seite durch Spielstarts weggefallen ist, nicht im Leeren. Welche
    Seite tatsaechlich geliefert wurde, steht deshalb mit in der Antwort.

    Mit "?name=" nur das Spiel genau dieses Namens (oder keins) -- dafuer
    schlaegt die Beitreten-Seite das Punktelimit nach, unabhaengig davon,
    auf welcher Seite das Spiel stuende.
    """

    permission_classes = [IsAuthenticated, HasPlayerRole]

    PAGE_SIZE = 20

    def get(self, request: Request) -> Response:
        games: QuerySet[Game] = Game.objects.filter(status=GameStatus.LOBBY).order_by("id")
        name: str | None = request.query_params.get("name")
        if name is not None:
            games = games.filter(name=name)
        page: Page[Game] = Paginator(games, self.PAGE_SIZE).get_page(request.query_params.get("page"))
        serializer: GameListSerializer = GameListSerializer(page.object_list, many=True)
        return Response(
            {
                "games": serializer.data,
                "count": page.paginator.count,
                "page": page.number,
                "page_size": self.PAGE_SIZE,
            }
        )


class CurrentGameView(APIView):
    """
    Liefert das Spiel des anfragenden Spielers, falls vorhanden, samt
    Status -- fuer die Lobby und die "Aktuelles Spiel"-Seite im Frontend
    (siehe radish/frontend/src/app/pages/games/lobby/ bzw. current/). Die
    Lobby pollt diesen Endpunkt.

    {"game": null}, wenn der Spieler gerade kein Spiel hat -- das ist der
    normale, haeufigste Fall (nicht jeder eingeloggte Spieler ist gerade in
    einem Spiel), deshalb 200 statt 404. Legt dabei anders als
    GameCreateView/GameJoinView auch KEIN neues Player-Objekt an (dieselbe
    Ueberlegung wie bei LeaveGameView unten): ohne Player-Objekt kann es ohnehin kein
    Spiel geben.
    """

    permission_classes = [IsAuthenticated, HasPlayerRole]

    def get(self, request: Request) -> Response:
        # Eine Abfrage: das Spiel direkt ueber den User statt erst den Player
        # zu suchen, dazu alles, was GameDetailSerializer liest (Spieler-
        # Kennungen und Armeenamen). Ohne Player-Objekt findet sie kein Spiel.
        user: User | AnonymousUser = request.user
        game: Game | None = (
            Game.objects.select_related("host", "second_player", "host_army", "second_player_army")
            .filter(Q(host__user=user) | Q(second_player__user=user))
            .first()
        )
        if game is None:
            return Response({"game": None})

        # Der anfragende Spieler steckt schon im Spiel: Host oder zweiter Spieler.
        player: Player | None = game.host if game.host.user_id == user.id else game.second_player
        return Response({"game": GameDetailSerializer(game, context={"player": player}).data})


def _leave_game(game: Game, player: Player) -> None:
    """
    Nimmt "player" aus dem (gesperrten) Spiel heraus -- die Faelle stehen
    bei LeaveGameView. Erwartet dieselbe Transaktion wie end_started_game.
    """
    if game.status != GameStatus.LOBBY:
        # Gestartet -- das Spiel endet fuer beide.
        end_started_game(game)
    elif game.second_player_id == player.id:
        game.second_player = None
        game.second_player_army = None
        game.save(update_fields=["second_player", "second_player_army"])
    elif game.second_player_id:
        # Host verlaesst, zweiter Spieler ruecht auf -- mit seiner Armee.
        # second_player_id ist gesetzt, second_player also nicht None.
        assert game.second_player is not None
        game.host = game.second_player
        game.host_army = game.second_player_army
        game.second_player = None
        game.second_player_army = None
        game.save(
            update_fields=["host", "host_army", "second_player", "second_player_army"]
        )
    else:
        # Host verlaesst, niemand sonst da -- Spiel endet.
        server: GameServer = game.server
        game.delete()
        server.is_occupied = False
        server.save(update_fields=["is_occupied"])


class LeaveGameView(APIView):
    """
    Laesst den anfragenden Spieler sein aktuelles Spiel verlassen (siehe
    "Verlassen"-Button auf der "Aktuelles Spiel"-Seite im Frontend,
    radish/frontend/src/app/pages/games/current/).

    Ist das Spiel schon gestartet (RUNNING), wird es fuer
    beide Spieler beendet -- so, als haetten beide es verlassen: das Spiel
    wird geloescht, sein GameServer freigegeben und die Instanz angewiesen,
    abzubrechen (instance_client.cancel_game, nach dem Commit).

    In der Lobby zwei Faelle, je nachdem, wer verlaesst:
    - Der zweite Spieler verlaesst: second_player wird nur geleert, das
      Spiel selbst bleibt bestehen -- der Host spielt weiter, der Platz
      wird wieder frei (GameJoinView laesst dann wieder jemanden beitreten,
      das prueft second_player_id ohnehin schon).
    - Der Host verlaesst: gibt es einen zweiten Spieler, ruecht der zum
      Host auf -- das Spiel bleibt fuer ihn bestehen, statt ihn ebenfalls
      rauszuwerfen. Gibt es keinen zweiten Spieler, ist niemand mehr da,
      der weiterspielen koennte: das Spiel wird geloescht und sein
      GameServer wieder freigegeben (is_occupied=False), damit ihn das
      naechste GameCreateView wieder vergeben kann.

    select_for_update() + transaction.atomic() aus demselben Grund wie bei
    GameCreateView/GameJoinView: zwei gleichzeitige Anfragen (z.B. wenn
    Host und zweiter Spieler im selben Moment verlassen) duerfen sich
    nicht gegenseitig aushebeln.

    Anders als GameCreateView/GameJoinView legt dieser Endpunkt fuer einen
    Nutzer ohne Player-Objekt KEINEN neuen Player an (dieselbe Ueberlegung
    wie bei CurrentGameView oben): ohne Player-Objekt kann er ohnehin in keinem
    Spiel sein.

    In der Lobby hat die Instanz noch nichts vom Spiel erfahren -- dort
    geht deshalb auch keine Nachricht an sie.
    """

    permission_classes = [IsAuthenticated, HasPlayerRole]

    def post(self, request: Request) -> Response:
        # IsAuthenticated laesst keinen AnonymousUser durch.
        assert not isinstance(request.user, AnonymousUser)
        player: Player
        try:
            player = Player.objects.get(user=request.user)
        except Player.DoesNotExist:
            return Response(
                {"detail": "Kein Spieler-Profil vorhanden -- noch nie ein Spiel erstellt oder ihm beigetreten."},
                status=status.HTTP_404_NOT_FOUND,
            )

        with transaction.atomic():
            game: Game | None = (
                Game.objects.select_for_update()
                .select_related("server")
                .filter(Q(host=player) | Q(second_player=player))
                .first()
            )
            if game is None:
                return Response(
                    {"detail": "Spieler ist in keinem Spiel."},
                    status=status.HTTP_409_CONFLICT,
                )

            _leave_game(game, player)

        return Response({"status": "left"})


class PlayerStateView(APIView):
    """
    Platzhalter-Endpunkt, der den aktuellen Zustand des anfragenden
    Spielers liefert.

    Welcher Spieler das ist (Session? Token? Uuid im Kommando-Header,
    wie im C-Server unter radish/game-server-core/?) und was sein Zustand
    umfasst, ist noch offen -- dieser View existiert erstmal nur, damit
    der Endpunkt aufrufbar ist.
    """
    #
    # TODO: Sobald OIDC umgesetzt ist muss dieser Endpunkt vernuenftig umgesetzt werden.
    #

    permission_classes = [IsAuthenticated, HasPlayerRole]

    def get(self, request: Request) -> Response:
        return Response({"status": "ok"})
