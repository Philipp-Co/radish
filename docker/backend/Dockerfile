# Django-Backend (radish/backend/): die Matchmaking-API (Game/GameServer/
# Player unter api/client/, api/servers/) sowie Login/Logout und die
# JWT-Absicherung gegen Keycloak (siehe api/auth_views.py, api/authentication.py).
# Liefert ausserdem das gesamte Frontend aus -- das Angular-Frontend
# (radish/frontend/, Login/Spielübersicht/Spielansicht) unter "/" und den
# Wasm-Client (radish/web/) unter "/wasm/" (siehe radish/backend/web/) --
# es gibt dafuer bewusst keinen separaten nginx-Container mehr.
#
# Drei Stages: "frontend-builder" baut nur das Angular-Bundle (Node),
# "builder" nur die Python-Abhaengigkeiten (siehe unten), das eigentliche
# Image uebernimmt aus beiden nur das fertige Ergebnis -- weder Node noch
# Rust/gcc landen im laufenden Container.
#
# Der Python-Teil ist zweistufig: simplejwts [crypto]-Extra zieht
# "cryptography" nach sich, und dafuer gibt es
# fuer Alpine (musl statt glibc) keine vorgefertigten Wheels -- der Build
# braucht Rust/gcc/die -dev-Header, das fertige Image nur noch die
# Laufzeitbibliotheken.
#
# ASGI statt WSGI: config/asgi.py routet HTTP weiter an Django wie
# bisher, WebSocket-Verbindungen (siehe radish/backend/api/routing.py,
# api/consumers.py) laufen ueber Channels. Ausgeliefert wird beides ueber
# Daphne (siehe entrypoint.sh) -- weiterhin kein Reverse-Proxy davor.
FROM node:20-alpine AS frontend-builder
WORKDIR /frontend-src
COPY radish/frontend/package.json radish/frontend/package-lock.json* ./
RUN npm install
COPY radish/frontend/ ./
RUN npm run build -- --configuration production


FROM python:3.12-alpine AS builder

RUN apk add --no-cache \
    gcc \
    musl-dev \
    python3-dev \
    libffi-dev \
    openssl-dev \
    cargo \
    rust

COPY radish/backend/requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir --prefix=/install -r /tmp/requirements.txt


FROM python:3.12-alpine

RUN apk add --no-cache \
    libffi \
    openssl

COPY --from=builder /install /usr/local

WORKDIR /app

# Ohne das puffert Python stdout blockweise, sobald es nicht an ein
# Terminal haengt (also praktisch immer in Docker) -- print()-Ausgaben
# aus dem Anwendungscode (siehe api/consumers.py) wuerden sonst erst mit
# grosser Verzoegerung oder gar nicht in "docker compose logs" auftauchen,
# waehrend Daphnes eigene Zugriffslogs (WSCONNECT/WSREJECT/...) ueber das
# logging-Modul laufen und davon nicht betroffen sind.
ENV PYTHONUNBUFFERED=1

COPY radish/backend/ /app/

# Wasm-Client fuer die Django-App "web" (siehe radish/backend/web/views.py,
# WASM_DIR = Geschwisterverzeichnis von BASE_DIR): lokal liegt radish/web/
# neben radish/backend/, im Image muss das also parallel zu /app nach /web --
# sonst laeuft views.wasm_file() im Container ins Leere (404), obwohl es
# lokal (ausserhalb von Docker) funktioniert.
COPY radish/web/ /web/

# Gebautes Angular-Frontend (siehe Stage "frontend-builder" oben) --
# radish/backend/web/views.py erwartet es unter FRONTEND_DIST_DIR =
# Geschwisterverzeichnis von BASE_DIR, genau wie /web/ oben.
COPY --from=frontend-builder /frontend-src/dist/radish-frontend/browser /frontend/dist/radish-frontend/browser

COPY docker/backend/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 8000

ENTRYPOINT ["/entrypoint.sh"]
