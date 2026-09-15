"""
Endpunkte fuer den Adminbereich des Frontends (siehe radish/frontend/src/app/
pages/games/admin/) -- getrennt von client_views.py (Spieler-Endpunkte) und
server_views.py (Registrierung durch den technischen Server-Account): hier
geht es ausschliesslich um lesende Zugriffe fuer menschliche Admins mit der
Keycloak-Realm-Rolle "Radish-Admin" (siehe permissions.HasAdminRole).
"""

from rest_framework.permissions import IsAuthenticated
from rest_framework.response import Response
from rest_framework.views import APIView

from .models import GameServer
from .permissions import HasAdminRole
from .serializers import GameServerSerializer


class AdminServerListView(APIView):
    """
    Listet alle aktuell angemeldeten Game-Server auf (siehe
    server_views.ServerView, das dieselben Server registriert/entfernt).

    Eigener Endpunkt statt Wiederverwendung von GET /api/servers/: Letzterer
    ist fuer den technischen Server-Account gedacht (nur IsAuthenticated,
    siehe dortigen Docstring) und bewusst nicht an eine Rolle geknuepft --
    hier soll stattdessen ausschliesslich "Radish-Admin" herankommen.
    """

    permission_classes = [IsAuthenticated, HasAdminRole]

    def get(self, request):
        servers = GameServer.objects.all().order_by("name")
        serializer = GameServerSerializer(servers, many=True)
        return Response({"servers": serializer.data})
