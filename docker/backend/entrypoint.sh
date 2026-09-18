#!/bin/sh

python manage.py migrate --noinput

# Timeouts/Limits bewusst explizit statt Daphne-Default:
# --websocket_timeout -1: WebSocket-Verbindungen sollen beliebig lange
#   offen bleiben (die UDP-Bruecke in api/consumers.py.EchoConsumer laeuft,
#   solange das Spiel laeuft -- keine feste Obergrenze gewuenscht).
# --websocket_connect_timeout 30: 30s Zeit fuers Handshake (inkl. der
#   Keycloak-Token-Pruefung in api/middleware.py).
# --http-timeout 5 / --application-close-timeout 5: kurze Timeouts fuer
#   HTTP-Requests bzw. zum sauberen Beenden nach einem Disconnect.
# --websocket-max-message-size / --websocket-max-frame-size 1024: klein
#   gehalten, um unauthentifizierten Speicherverbrauch durch grosse
#   WebSocket-Nachrichten zu begrenzen.
exec daphne \
    -b 0.0.0.0 \
    -p 8000 \
    --websocket_timeout -1 \
    --websocket_connect_timeout 30 \
    --http-timeout 5 \
    --application-close-timeout 5 \
    --websocket-max-message-size 1024 \
    --websocket-max-frame-size 1024 \
    config.asgi:application
