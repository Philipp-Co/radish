"""
WebSocket-Pendant zu KeycloakJWTAuthentication (siehe authentication.py):
prueft dasselbe Keycloak-Access-Token, aber beim WebSocket-Handshake statt
bei einem DRF-Request.

Browser-WebSockets koennen beim Handshake keine eigenen Header mitschicken
(die JS-WebSocket-API erlaubt das nicht) -- das Token kommt deshalb als
Query-Parameter ("wss://.../ws/echo/?token=..."), nicht als
Authorization-Header.
"""

from urllib.parse import parse_qs

from channels.db import database_sync_to_async
from django.contrib.auth.models import AnonymousUser
from rest_framework_simplejwt.exceptions import InvalidToken, TokenError

from .authentication import KeycloakJWTAuthentication


@database_sync_to_async
def _resolve_user(raw_token):
    """
    Validiert (JWKS-Pruefung, siehe SIMPLE_JWT in settings.py) und legt bei
    Bedarf den User an (get_or_create_user_from_claims, siehe keycloak.py) --
    beides synchron/ORM-lastig, deshalb database_sync_to_async statt einer
    async def hier.
    """
    auth = KeycloakJWTAuthentication()
    try:
        validated_token = auth.get_validated_token(raw_token)
        return auth.get_user(validated_token)
    except (InvalidToken, TokenError):
        return AnonymousUser()


class KeycloakTokenAuthMiddleware:
    """
    ASGI-Middleware nach demselben Muster wie channels.auth.AuthMiddlewareStack,
    nur gegen Keycloak-Tokens statt gegen die Django-Session (die hier fuer
    api/-Endpunkte ohnehin nicht verwendet wird, siehe authentication.py).

    Setzt scope["user"]. Fehlt das Token oder ist es ungueltig, ist das
    AnonymousUser -- genau wie bei einem nicht eingeloggten HTTP-Request.
    Ob eine Verbindung ohne authentifizierten User abgelehnt wird, entscheidet
    der jeweilige Consumer (siehe consumers.py), nicht diese Middleware.
    """

    def __init__(self, app):
        self.app = app

    async def __call__(self, scope, receive, send):
        query_string = scope.get("query_string", b"").decode()
        token = parse_qs(query_string).get("token", [None])[0]

        scope["user"] = await _resolve_user(token) if token else AnonymousUser()
        return await self.app(scope, receive, send)
