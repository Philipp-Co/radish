"""
ASGI config for config project.

Routet HTTP wie gehabt an Django (django_asgi_app unten), WebSocket-
Verbindungen dagegen an Channels: api/routing.py listet die Endpunkte,
api/consumers.py die Handler, api/middleware.py prueft beim Handshake das
Keycloak-Access-Token (siehe dort fuer den Grund: dieselbe Pruefung wie bei
der REST-API, siehe api/authentication.py, nur eben nicht aus einem
Authorization-Header, sondern aus dem Query-String).

For more information on this file, see
https://docs.djangoproject.com/en/5.2/howto/deployment/asgi/
https://channels.readthedocs.io/en/latest/topics/routing.html
"""

import os

from channels.routing import ProtocolTypeRouter, URLRouter
from channels.security.websocket import AllowedHostsOriginValidator
from django.core.asgi import get_asgi_application

os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'config.settings')

# Muss vor den Importen unten stehen: api.routing/api.middleware ziehen ueber
# "from . import consumers" bzw. "from .authentication import ..."
# Django-Modelle nach sich, die erst nach get_asgi_application() (stoesst
# django.setup() an) importierbar sind.
django_asgi_app = get_asgi_application()

from api.middleware import KeycloakTokenAuthMiddleware  # noqa: E402
from api.routing import websocket_urlpatterns  # noqa: E402

application = ProtocolTypeRouter({
    'http': django_asgi_app,
    # AllowedHostsOriginValidator: prueft den Origin-Header des Handshakes
    # gegen ALLOWED_HOSTS (settings.py) -- anders als bei fetch()/XHR
    # erzwingen Browser bei WebSockets keine Same-Origin-Policy, das muss
    # also serverseitig passieren.
    'websocket': AllowedHostsOriginValidator(
        KeycloakTokenAuthMiddleware(
            URLRouter(websocket_urlpatterns)
        )
    ),
})
