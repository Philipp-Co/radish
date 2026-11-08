from rest_framework.permissions import BasePermission
from rest_framework.request import Request
from rest_framework.views import APIView

from api.interfaces.auth.roles.realm_roles import _realm_roles


class HasAdminRole(BasePermission):
    """
    Laesst nur Nutzer mit der Keycloak-Realm-Rolle "Radish-Admin" durch.

    Gedacht fuer den Adminbereich (siehe admin_views.py): Liste der
    angemeldeten Game-Server u.ae. -- bewusst eine eigene Rolle statt
    "Player", da ein Admin nicht zwangslaeufig auch Spieler sein muss (auch
    wenn "Radish-Admin" hier ueber default-roles-radish zusaetzlich immer
    "Player" mitbekommt, siehe docker/keycloak/realm-radish.json). Genau wie
    HasPlayerRole setzt das IsAuthenticated voraus und muss deshalb immer
    zusammen damit verwendet werden: permission_classes = [IsAuthenticated,
    HasAdminRole].
    """

    message = "Erfordert die Keycloak-Rolle 'Radish-Admin'."

    def has_permission(self, request: Request, view: APIView) -> bool:
        return "Radish-Admin" in _realm_roles(request)
