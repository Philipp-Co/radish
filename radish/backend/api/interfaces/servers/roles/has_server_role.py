from rest_framework.permissions import BasePermission
from rest_framework.request import Request
from rest_framework.views import APIView

from ...auth.roles.realm_roles import _realm_roles


class HasServerRole(BasePermission):
    """
    Laesst nur Nutzer mit der Keycloak-Realm-Rolle "Technischer-User-Server"
    durch -- den Service-Account der Game-Server (radish-game-server, siehe
    docker/keycloak/realm-radish.json).

    Gedacht fuer Endpunkte, mit denen eine Spielinstanz den Zustand eines
    Spiels aendert (siehe server_views.GameFinishedView): ein Spieler darf
    nicht selbst erklaeren, dass ein Spiel vorbei ist.

    Wie HasPlayerRole nur zusammen mit IsAuthenticated: permission_classes =
    [IsAuthenticated, HasServerRole].
    """

    message = "Erfordert die Keycloak-Rolle 'Technischer-User-Server'."

    def has_permission(self, request: Request, view: APIView) -> bool:
        return "Technischer-User-Server" in _realm_roles(request)
