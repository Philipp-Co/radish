from rest_framework.permissions import BasePermission
from rest_framework.request import Request
from rest_framework.views import APIView

from ...auth.roles.realm_roles import _realm_roles


class HasPlayerRole(BasePermission):
    """
    Laesst nur Nutzer mit der Keycloak-Realm-Rolle "Player" durch.

    Gedacht fuer die client/-Endpunkte: die duerfen nur menschliche
    Spieler nutzen, keine technischen Server-Accounts (Rolle
    "Technischer-User-Server", siehe docker/keycloak/realm-radish.json).
    Setzt IsAuthenticated voraus -- ohne gueltiges Token gibt es kein
    request.auth und damit auch keine Rollen, aber diese Permission allein
    wuerde in dem Fall nur 403 statt 401 liefern. Deshalb immer zusammen
    mit IsAuthenticated einsetzen: permission_classes = [IsAuthenticated,
    HasPlayerRole].
    """

    message = "Erfordert die Keycloak-Rolle 'Player'."

    def has_permission(self, request: Request, view: APIView) -> bool:
        return "Player" in _realm_roles(request)
