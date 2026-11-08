"""
WebSocket-Pendant zu KeycloakJWTAuthentication (siehe authentication.py):
prueft dasselbe Keycloak-Access-Token, aber beim WebSocket-Handshake statt
bei einem DRF-Request.

Browser-WebSockets koennen beim Handshake keine eigenen Header mitschicken
(die JS-WebSocket-API erlaubt das nicht) -- das Token kommt deshalb als
Query-Parameter ("wss://.../ws/echo/?token=..."), nicht als
Authorization-Header.
"""

from typing import NotRequired
from urllib.parse import parse_qs

from asgiref.typing import ASGI3Application, ASGIReceiveCallable, ASGISendCallable, WebSocketScope
from channels.db import database_sync_to_async

# Nur fuer Type Hints: das Projekt nutzt Djangos Standard-User-Model (kein
# AUTH_USER_MODEL), get_user_model() liefert also genau diese Klasse.
# pylint: disable-next=imported-auth-user
from django.contrib.auth.models import AnonymousUser, User
from rest_framework_simplejwt.exceptions import InvalidToken, TokenError
from rest_framework_simplejwt.tokens import Token

from api.interfaces.auth.authentication import KeycloakJWTAuthentication


class UserWebSocketScope(WebSocketScope):
    """
    WebSocket-Scope, wie ihn diese Middleware weitergibt: mit dem User, den
    die Consumer als self.scope["user"] lesen. NotRequired, weil er beim
    Eintreffen noch fehlt und erst hier gesetzt wird.
    """

    user: NotRequired[User | AnonymousUser]


@database_sync_to_async
def _resolve_user(raw_token: str) -> User | AnonymousUser:
    """
    Validiert (JWKS-Pruefung, siehe SIMPLE_JWT in settings.py) und legt bei
    Bedarf den User an (get_or_create_user_from_claims, siehe keycloak.py) --
    beides synchron/ORM-lastig, deshalb database_sync_to_async statt einer
    async def hier.
    """
    auth: KeycloakJWTAuthentication = KeycloakJWTAuthentication()
    try:
        # simplejwt annotiert bytes, das Token aus dem Query-String ist aber str
        # -- der Token-Konstruktor und PyJWT verarbeiten beides.
        validated_token: Token = auth.get_validated_token(raw_token)  # type: ignore[arg-type]
        return auth.get_user(validated_token)
    except (InvalidToken, TokenError):
        return AnonymousUser()


# ASGI-Middleware: die Schnittstelle besteht nur aus __call__.
# pylint: disable-next=too-few-public-methods
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

    def __init__(self, app: ASGI3Application) -> None:
        self.app = app

    async def __call__(
        self,
        scope: UserWebSocketScope,
        receive: ASGIReceiveCallable,
        send: ASGISendCallable,
    ) -> None:
        query_string: str = scope.get("query_string", b"").decode()
        token: str | None = parse_qs(query_string).get("token", [None])[0]

        scope["user"] = await _resolve_user(token) if token else AnonymousUser()
        return await self.app(scope, receive, send)
