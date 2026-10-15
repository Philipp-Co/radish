"""
Endpunkte, um verfuegbare Server bekannt zu machen bzw. wieder zu entfernen.

Eigene Datei, getrennt von views.py: dort stehen die generischen
Platzhalter-Endpunkte (POST/SSE), hier alles rund um GameServer.
"""

import logging

from django.db import transaction
from rest_framework import status
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView

from .client_views import end_started_game
from .models import Game, GameServer, GameStatus
from .permissions import HasServerRole
from .serializers import GameFinishedSerializer, GameServerSerializer

logger = logging.getLogger(__name__)


class ServerView(APIView):
    """
    Macht einen Server bekannt (POST), listet alle bekannten Server auf
    (GET) und entfernt einen Server wieder (DELETE).

    Registrieren und Auflisten brauchen keinen Namen in der URL (Name
    steht im Body bzw. betrifft alle Eintraege), Entfernen identifiziert
    den Server ueber genau diesen Namen als URL-Parameter -- deshalb ist
    dieselbe Klasse unter zwei Pfaden eingehaengt (siehe urls.py):
    "servers/" fuer POST/GET, "servers/<name>/" fuer DELETE.

    Wie beim Registrieren nur IsAuthenticated (keine HasPlayerRole): das
    ist fuer den technischen Server-Account gedacht (siehe permissions.py),
    nicht fuer Spieler-Clients -- die Antwort enthaelt bewusst Adresse und
    Port, anders als z.B. GameDetailSerializer fuer Spiele.
    """

    permission_classes = [IsAuthenticated]

    def get(self, request, name=None):
        """
        Liefert registrierte Server, inklusive Adresse, Port und
        Belegungsstatus (is_occupied).

        Ohne "name" (Pfad "servers/") alle Server als Liste. Die Klasse
        haengt aber auch unter "servers/<name>/" (siehe urls.py, wegen
        DELETE) -- ein GET dorthin liefert deshalb statt der Liste genau
        den einen Server mit diesem Namen (404, falls es ihn nicht gibt),
        anstatt mit einem TypeError auf den unerwarteten URL-Parameter
        zu reagieren.
        """
        if name is not None:
            try:
                server = GameServer.objects.get(name=name)
            except GameServer.DoesNotExist:
                return Response(status=status.HTTP_404_NOT_FOUND)
            serializer = GameServerSerializer(server)
            return Response(serializer.data)

        servers = GameServer.objects.all().order_by("name")
        serializer = GameServerSerializer(servers, many=True)
        return Response({"servers": serializer.data})

    def post(self, request):
        """
        Macht einen Server bekannt: legt ihn mit Name, Adresse und Port an.

        Der Name muss eindeutig sein (siehe models.GameServer) -- ein
        Server mit bereits vergebenem Namen liefert 400 statt eines
        zweiten Eintrags.
        """
        serializer = GameServerSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        serializer.save()
        return Response(serializer.data, status=status.HTTP_201_CREATED)

    def delete(self, request, name):
        """
        Entfernt einen zuvor bekannt gemachten Server wieder, identifiziert
        ueber seinen Namen (nicht ueber die Datenbank-ID).
        """
        deleted_count, _ = GameServer.objects.filter(name=name).delete()
        if not deleted_count:
            return Response(status=status.HTTP_404_NOT_FOUND)
        return Response(status=status.HTTP_204_NO_CONTENT)



class GameFinishedView(APIView):
    """
    Die Spielinstanz meldet, dass das Spiel auf ihr vorbei ist:

        POST /api/servers/<name>/game-finished/
        {"winner": "<Spieler-Id>", "loser": "<Spieler-Id>", "game-instance": "<name>"}

    Bei einem Unentschieden sind winner und loser beide null.

    Die Spieler-Ids sind Player.identifier -- dieselbe Kennung, mit der die
    Instanz die Spieler aus dem Spielstart kennt (instance_client
    .game_start_request). "game-instance" ist der Name des Servers und muss
    zu dem in der URL passen.

    Danach endet das Spiel wie beim Verlassen (client_views.end_started_game):
    das Game verschwindet, der Server wird frei, die Instanz verwirft das Spiel
    und die Zugangscodes. Das Ergebnis wird nicht aufbewahrt, nur ins Log
    geschrieben.

    Nur fuer den Service-Account der Game-Server (HasServerRole). Antworten:
    400 fuer einen ungueltigen Body, eine fremde Instanz im Body oder Ids, die
    nicht die beiden Spieler des Spiels sind; 404 fuer einen unbekannten
    Server; 409, wenn auf ihm kein gestartetes Spiel laeuft -- auch fuer eine
    zweite Meldung desselben Endes.
    """

    permission_classes = [IsAuthenticated, HasServerRole]

    def post(self, request, name):
        serializer = GameFinishedSerializer(data=request.data)
        serializer.is_valid(raise_exception=True)
        data = serializer.validated_data

        if data["game-instance"] != name:
            return Response(
                {"game-instance": ["Passt nicht zum Server in der URL."]},
                status=status.HTTP_400_BAD_REQUEST,
            )

        with transaction.atomic():
            server = GameServer.objects.select_for_update().filter(name=name).first()
            if server is None:
                return Response(status=status.HTTP_404_NOT_FOUND)

            # Gesperrt wird nur das Game selbst: second_player ist nullable, und
            # FOR UPDATE geht nicht auf die Seite eines Outer Joins.
            game = (
                Game.objects.select_for_update(of=("self",))
                .select_related("server", "host", "second_player")
                .filter(server=server)
                .first()
            )
            if game is None or game.status != GameStatus.RUNNING:
                return Response(
                    {"detail": "Auf diesem Server laeuft kein gestartetes Spiel."},
                    status=status.HTTP_409_CONFLICT,
                )

            draw = data["winner"] is None
            players = {game.host.identifier}
            if game.second_player is not None:
                players.add(game.second_player.identifier)
            if not draw and {data["winner"], data["loser"]} != players:
                return Response(
                    {"detail": "winner und loser muessen die beiden Spieler dieses Spiels sein."},
                    status=status.HTTP_400_BAD_REQUEST,
                )

            if draw:
                logger.info("Spiel '%s' auf %s vorbei: unentschieden", game.name, name)
            else:
                logger.info(
                    "Spiel '%s' auf %s vorbei: gewonnen hat %s, verloren %s",
                    game.name, name, data["winner"], data["loser"],
                )
            end_started_game(game)

        return Response({"status": "finished"})
