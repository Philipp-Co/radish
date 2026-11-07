#!/bin/sh

# Migriert wird vorher vom Service "backend-migrate" (siehe
# docker-compose.yaml, docker/backend-migrate/), das Backend darf das Schema
# gar nicht aendern (Benutzer radish_app, siehe
# docker/backend-db/init/01-roles.sh). Hier nur die Pruefung: fehlt eine
# Migration (oder ist die Datenbank nicht erreichbar), bricht der Container
# ab, statt daphne auf einem veralteten Schema zu starten.
set -e

python manage.py migrate --check

# Timeouts/Limits bewusst explizit statt Daphne-Default:
# --websocket_timeout -1: WebSocket-Verbindungen sollen beliebig lange
#   offen bleiben (die UDP-Bruecke in api/interfaces/player/views/consumers.py.EchoConsumer laeuft,
#   solange das Spiel laeuft -- keine feste Obergrenze gewuenscht).
# --websocket_connect_timeout 30: 30s Zeit fuers Handshake (inkl. der
#   Keycloak-Token-Pruefung in api/interfaces/auth/middleware.py).
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
