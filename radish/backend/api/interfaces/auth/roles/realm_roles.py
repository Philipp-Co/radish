"""
Keycloak-Realm-Rollen fuer die eigenen DRF-Permissions, je eine pro
Bereich: HasPlayerRole (interfaces/player/), HasAdminRole
(interfaces/admin/), HasServerRole (interfaces/servers/).

Die Rolle eines Nutzers steht nicht am Django-User (der hat nur username =
Keycloaks "sub", siehe keycloak.get_or_create_user_from_claims), sondern
ausschliesslich im validierten Access-Token selbst, in der Klaim
"realm_access.roles". KeycloakJWTAuthentication (siehe authentication.py)
sorgt dafuer, dass DRF dieses validierte Token als request.auth bereitstellt
-- genau das liest _realm_roles hier aus.
"""

from typing import cast

from rest_framework.request import Request
from rest_framework_simplejwt.tokens import Token

from ..keycloak import RealmAccess


def _realm_roles(request: Request) -> list[str]:
    """
    Liest die Keycloak-Realm-Rollen aus dem validierten Access-Token.

    request.auth ist das simplejwt-Token-Objekt (rest_framework_simplejwt
    .tokens.Token), das sich wie ein Mapping auf seine JWT-Klaims verhaelt.
    Ohne erfolgreiche Authentifizierung (z.B. bei AllowAny-Views ohne
    Token) ist request.auth None -- dann gibt es auch keine Rollen.
    """
    # DRF typisiert request.auth mit seinem eigenen authtoken-Token; hier
    # setzt es aber KeycloakJWTAuthentication, also ein simplejwt-Token.
    token: Token | None = cast(Token | None, request.auth)
    if token is None:
        return []
    # Token.get ist in simplejwt Any; der Klaim hat die Form von RealmAccess.
    # RealmAccess() ist zur Laufzeit dasselbe leere dict wie vorher "{}", ein
    # nacktes "{}" leitet mypy hinter "or" aber als dict[Any, Any] her.
    realm_access: RealmAccess = token.get("realm_access", {}) or RealmAccess()
    return realm_access.get("roles", []) or []
