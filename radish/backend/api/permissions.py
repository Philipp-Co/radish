"""
Eigene DRF-Permissions rund um Keycloak-Realm-Rollen.

Die Rolle eines Nutzers steht nicht am Django-User (der hat nur username =
Keycloaks "sub", siehe keycloak.get_or_create_user_from_claims), sondern
ausschliesslich im validierten Access-Token selbst, in der Klaim
"realm_access.roles". KeycloakJWTAuthentication (siehe authentication.py)
sorgt dafuer, dass DRF dieses validierte Token als request.auth bereitstellt
-- genau das lesen die Permissions hier aus.
"""

from rest_framework.permissions import BasePermission


def _realm_roles(request):
    """
    Liest die Keycloak-Realm-Rollen aus dem validierten Access-Token.

    request.auth ist das simplejwt-Token-Objekt (rest_framework_simplejwt
    .tokens.Token), das sich wie ein Mapping auf seine JWT-Klaims verhaelt.
    Ohne erfolgreiche Authentifizierung (z.B. bei AllowAny-Views ohne
    Token) ist request.auth None -- dann gibt es auch keine Rollen.
    """
    token = request.auth
    if token is None:
        return []
    realm_access = token.get("realm_access", {}) or {}
    return realm_access.get("roles", []) or []


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

    def has_permission(self, request, view):
        return "Player" in _realm_roles(request)


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

    def has_permission(self, request, view):
        return "Radish-Admin" in _realm_roles(request)
