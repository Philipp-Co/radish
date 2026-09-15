"""
Endpunkte, um verfuegbare Server bekannt zu machen bzw. wieder zu entfernen.

Eigene Datei, getrennt von views.py: dort stehen die generischen
Platzhalter-Endpunkte (POST/SSE), hier alles rund um GameServer.
"""

from rest_framework import status
from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView

from .models import GameServer
from .serializers import GameServerSerializer


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
    nicht fuer Spieler-Clients -- die Antwort enthaelt bewusst IP und
    Port, anders als z.B. GameDetailSerializer fuer Spiele.
    """

    permission_classes = [IsAuthenticated]

    def get(self, request, name=None):
        """
        Liefert registrierte Server, inklusive IP, Port und
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
        Macht einen Server bekannt: legt ihn mit Name, IP und Port an.

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
