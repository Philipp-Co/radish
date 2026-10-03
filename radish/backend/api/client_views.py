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
WebSocket (siehe api/consumers.py, EchoConsumer).
"""

from django.db import transaction
from django.db.models import Q
from rest_framework import status
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView

from . import access, instance_client
from .models import Game, GameServer, GameStatus, Player, armies_with_costs
from .permissions import HasPlayerRole
from .serializers import (
    GameCreateSerializer,
    GameDetailSerializer,
    GameJoinSerializer,
    GameListSerializer,
)


def _sync_player_name(player, request):
    """
    Uebernimmt den Keycloak-Benutzernamen ("preferred_username" im
    validierten Access-Token, siehe permissions._realm_roles fuer
    request.auth) in Player.name, falls er sich geaendert hat.
    """
    token = request.auth
    name = (token.get("preferred_username") if token is not None else None) or ""
    if name and name != player.name:
        player.name = name
        player.save(update_fields=["name"])


def _get_or_create_player(request):
    """
    Loest den durch Keycloak authentifizierten Django-User (request.user,
    siehe KeycloakJWTAuthentication in api/authentication.py) zu seinem
    Player auf. Gibt es noch keinen, wird jetzt einer angelegt -- die
    zufaellige, oeffentlich sichtbare Kennung (Player.identifier) erzeugt
    Player.save() dabei weiterhin selbst.

    Anders als frueher hat der Client keinen Einfluss mehr darauf, welcher
    Player gemeint ist: das ergibt sich zwingend aus request.user, also aus
    dem validierten Access-Token -- ein player_identifier im Request-Body
    entfaellt komplett (siehe serializers.py).
    """
    player, _ = Player.objects.get_or_create(user=request.user)
    _sync_player_name(player, request)
    return player


def _army_for_game(player, army_id, points_limit):
    """
    Die Armee, mit der ein Spieler antreten will: sie muss ihm gehoeren,
    mindestens eine Einheit haben und darf hoechstens points_limit kosten.
    Liefert (army, None) oder (None, Fehler-Response).
    """
    army = armies_with_costs().filter(pk=army_id, player=player).first()
    if army is None:
        return None, Response(
            {"detail": "Armee nicht gefunden."}, status=status.HTTP_400_BAD_REQUEST
        )
    if not army.units.all():
        return None, Response(
            {"detail": f"Die Armee „{army.name}“ hat noch keine Einheiten."},
            status=status.HTTP_400_BAD_REQUEST,
        )
    cost = army.total_cost()
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

    def post(self, request):
        serializer = GameCreateSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        name = serializer.validated_data["name"]
        password = serializer.validated_data["password"]
        points_limit = serializer.validated_data["points_limit"]

        player = _get_or_create_player(request)

        already_in_game = Game.objects.filter(
            Q(host=player) | Q(second_player=player)
        ).exists()
        if already_in_game:
            return Response(
                {"detail": "Spieler ist bereits in einem Spiel."},
                status=status.HTTP_409_CONFLICT,
            )

        army, error = _army_for_game(player, serializer.validated_data["army_id"], points_limit)
        if error is not None:
            return error

        with transaction.atomic():
            server = (
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
            game = Game.objects.create(
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

    def post(self, request):
        serializer = GameJoinSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        name = serializer.validated_data["name"]
        password = serializer.validated_data["password"]

        player = _get_or_create_player(request)

        already_in_game = Game.objects.filter(
            Q(host=player) | Q(second_player=player)
        ).exists()
        if already_in_game:
            return Response(
                {"detail": "Spieler ist bereits in einem Spiel."},
                status=status.HTTP_409_CONFLICT,
            )

        with transaction.atomic():
            try:
                game = Game.objects.select_for_update().get(name=name)
            except Game.DoesNotExist:
                return Response(
                    {"detail": "Spiel nicht gefunden."},
                    status=status.HTTP_404_NOT_FOUND,
                )

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

    def post(self, request):
        player = Player.objects.filter(user=request.user).first()
        if player is None:
            return Response(
                {"detail": "Spieler ist in keinem Spiel."}, status=status.HTTP_409_CONFLICT
            )
        _sync_player_name(player, request)

        with transaction.atomic():
            # of=("self",): nur die Zeile des Spiels sperren -- second_player
            # ist nullable, und durch einen Outer Join sperrt Postgres nicht.
            game = (
                Game.objects.select_for_update(of=("self",))
                .select_related("server", "host", "second_player")
                .filter(Q(host=player) | Q(second_player=player))
                .first()
            )
            if game is None:
                return Response(
                    {"detail": "Spieler ist in keinem Spiel."},
                    status=status.HTTP_409_CONFLICT,
                )
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
    Liefert alle Spiele in der Lobby mit Name und Anzahl der bereits
    angemeldeten Spieler (1, solange nur der Ersteller da ist, sonst 2).
    Gestartete Spiele fehlen: denen kann ohnehin niemand mehr beitreten.
    """

    permission_classes = [IsAuthenticated, HasPlayerRole]

    def get(self, request):
        games = Game.objects.filter(status=GameStatus.LOBBY)
        serializer = GameListSerializer(games, many=True)
        return Response({"games": serializer.data})


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

    def get(self, request):
        try:
            player = Player.objects.get(user=request.user)
        except Player.DoesNotExist:
            return Response({"game": None})

        game = Game.objects.filter(Q(host=player) | Q(second_player=player)).first()
        if game is None:
            return Response({"game": None})

        return Response({"game": GameDetailSerializer(game, context={"player": player}).data})


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

    def post(self, request):
        try:
            player = Player.objects.get(user=request.user)
        except Player.DoesNotExist:
            return Response(
                {"detail": "Kein Spieler-Profil vorhanden -- noch nie ein Spiel erstellt oder ihm beigetreten."},
                status=status.HTTP_404_NOT_FOUND,
            )

        with transaction.atomic():
            game = (
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

            if game.status != GameStatus.LOBBY:
                # Gestartet -- das Spiel endet fuer beide.
                server = game.server
                game.delete()
                server.is_occupied = False
                server.save(update_fields=["is_occupied"])
                transaction.on_commit(lambda: instance_client.cancel_game(server))
            elif game.second_player_id == player.id:
                game.second_player = None
                game.second_player_army = None
                game.save(update_fields=["second_player", "second_player_army"])
            elif game.second_player_id:
                # Host verlaesst, zweiter Spieler ruecht auf -- mit seiner Armee.
                game.host = game.second_player
                game.host_army = game.second_player_army
                game.second_player = None
                game.second_player_army = None
                game.save(
                    update_fields=["host", "host_army", "second_player", "second_player_army"]
                )
            else:
                # Host verlaesst, niemand sonst da -- Spiel endet.
                server = game.server
                game.delete()
                server.is_occupied = False
                server.save(update_fields=["is_occupied"])

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

    def get(self, request):
        return Response({"status": "ok"})
