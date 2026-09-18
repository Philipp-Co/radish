"""
WebSocket-URL-Konfiguration -- das Pendant zu api/urls.py, nur fuer Channels
statt fuer normale Django-Views (siehe config/asgi.py, wo das eingehaengt
wird).
"""

from django.urls import re_path

from . import consumers

websocket_urlpatterns = [
    re_path(r"^ws/echo/$", consumers.EchoConsumer.as_asgi()),
]
