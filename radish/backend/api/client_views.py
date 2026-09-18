"""
Endpunkte fuer den Spielclient unter api/client/.

Ein Client kann hierueber ein Spiel erstellen, einem Spiel beitreten, es
wieder verlassen, die offenen Spiele auflisten, sein eigenes laufendes
Spiel abfragen und seinen eigenen Zustand abfragen. Erstellen, Beitreten,
Verlassen, Auflisten und das eigene laufende Spiel sind bereits echt
umgesetzt; Zustand ist noch Platzhalter (siehe dessen Docstring).

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

from .models import Game, GameServer, Player
from .permissions import HasPlayerRole
from .serializers import (
    GameCreateSerializer,
    GameDetailSerializer,
    GameJoinSerializer,
    GameListSerializer,
)


def _get_or_create_player(user):
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
    player, _ = Player.objects.get_or_create(user=user)
    return player


class GameCreateView(APIView):
    """
    Erstellt ein neues Spiel.

    Voraussetzungen:
    - Der Ersteller (request.user, also der authentifizierte Nutzer) ist
      noch in keinem anderen Spiel Host oder Mitspieler.
    - Es gibt mindestens einen freien GameServer (is_occupied=False); der
      wird dem neuen Spiel fest zugewiesen und sofort als belegt markiert.

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

        player = _get_or_create_player(request.user)

        already_in_game = Game.objects.filter(
            Q(host=player) | Q(second_player=player)
        ).exists()
        if already_in_game:
            return Response(
                {"detail": "Spieler ist bereits in einem Spiel."},
                status=status.HTTP_409_CONFLICT,
            )

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
                name=name, password=password, server=server, host=player
            )

        return Response(
            GameDetailSerializer(game).data, status=status.HTTP_201_CREATED
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
    - Das Spiel darf noch keinen zweiten Spieler haben.

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

        player = _get_or_create_player(request.user)

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

            if game.second_player_id:
                return Response(
                    {"detail": "Spiel ist bereits voll."},
                    status=status.HTTP_409_CONFLICT,
                )

            game.second_player = player
            game.save(update_fields=["second_player"])

        return Response(GameDetailSerializer(game).data, status=status.HTTP_200_OK)


class GameListView(APIView):
    """
    Liefert alle Spiele mit Name und Anzahl der bereits angemeldeten
    Spieler (1, solange nur der Ersteller da ist, sonst 2).

    Noch ohne Filter (z.B. nur Spiele mit freiem zweiten Platz) --
    zeigt aktuell wirklich alle Spiele, auch bereits volle.
    """

    permission_classes = [IsAuthenticated, HasPlayerRole]

    def get(self, request):
        games = Game.objects.all()
        serializer = GameListSerializer(games, many=True)
        return Response({"games": serializer.data})


class CurrentGameView(APIView):
    """
    Liefert das laufende Spiel des anfragenden Spielers, falls vorhanden --
    fuer die "Aktuelles Spiel"-Seite im Frontend (siehe radish/frontend/src/
    app/pages/games/current/).

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

        return Response({"game": GameDetailSerializer(game).data})


class LeaveGameView(APIView):
    """
    Laesst den anfragenden Spieler sein aktuelles Spiel verlassen (siehe
    "Verlassen"-Button auf der "Aktuelles Spiel"-Seite im Frontend,
    radish/frontend/src/app/pages/games/current/).

    Zwei Faelle, je nachdem, wer verlaesst:
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

    Schickt bewusst (noch) keine Nachricht an den eigentlichen Spielserver
    (radish/game-server-core/) -- das Ingame-Protokoll dafuer laeuft ueber
    den WebSocket (siehe api/consumers.py, EchoConsumer) und ist von dieser
    Matchmaking-API unabhaengig, siehe ApiService-Docstring im Frontend.
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

            if game.second_player_id == player.id:
                game.second_player = None
                game.save(update_fields=["second_player"])
            elif game.second_player_id:
                # Host verlaesst, zweiter Spieler ruecht auf.
                game.host = game.second_player
                game.second_player = None
                game.save(update_fields=["host", "second_player"])
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
