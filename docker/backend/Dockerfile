# Django-Backend (radish/backend/): die Matchmaking-API (Game/GameServer/
# Player unter api/client/, api/servers/) sowie Login/Logout und die
# JWT-Absicherung gegen Keycloak (siehe api/auth_views.py, api/authentication.py).
# Liefert ausserdem das gesamte Frontend aus -- das Angular-Frontend
# (radish/frontend/, Login/Spielübersicht/Spielansicht) unter "/" und den
# Wasm-Client (radish/web/) unter "/wasm/" (siehe radish/backend/web/) --
# es gibt dafuer bewusst keinen separaten nginx-Container mehr.
#
# Vier Stages: "frontend-builder" baut nur das Angular-Bundle (Node),
# "wasm-builder" nur den Wasm-Client (Emscripten), "builder" nur die
# Python-Abhaengigkeiten (siehe unten), das eigentliche Image uebernimmt aus
# allen nur das fertige Ergebnis -- weder Node noch Emscripten noch Rust/gcc
# landen im laufenden Container.
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


# Der Wasm-Client (radish/client/), frisch aus dem Quelltext -- nicht mehr das,
# was zufaellig gerade in radish/web/ liegt. Dieselbe Emscripten-Version wie das
# emsdk/ im Repository, sonst baut das Image einen anderen Client als der Host.
FROM emscripten/emsdk:6.0.5 AS wasm-builder

# protoc und protoc-gen-c als Host-Werkzeuge, wie im game-server-Image (siehe
# radish/cmake/protobuf-c.cmake).
RUN apt-get update \
    && apt-get install -y --no-install-recommends protobuf-compiler protobuf-c-compiler \
    && rm -rf /var/lib/apt/lists/*

# Die SDL-Ports vorab, in eigener Schicht: emcc laedt und uebersetzt sie sonst
# beim ersten Linken, und jede Aenderung unter radish/ wuerde das wiederholen.
RUN embuilder build sdl2 sdl2_ttf sdl2_image sdl2_image-png libpng zlib freetype harfbuzz

WORKDIR /src

# Kein zucchini/: unter Emscripten zieht radish/CMakeLists.txt es nicht.
COPY jsmn/ jsmn/
COPY protobuf-c/ protobuf-c/
COPY radish/ radish/

# Nur das Ziel "client": es schreibt client.js/client.wasm nach radish/web/
# (radish/client/CMakeLists.txt).
RUN emcmake cmake -S radish -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --target client


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
# lokal (ausserhalb von Docker) funktioniert. Aus der Stage "wasm-builder"
# oben, nicht aus dem Build-Kontext.
COPY --from=wasm-builder /src/radish/web/ /web/

# Gebautes Angular-Frontend (siehe Stage "frontend-builder" oben) --
# radish/backend/web/views.py erwartet es unter FRONTEND_DIST_DIR =
# Geschwisterverzeichnis von BASE_DIR, genau wie /web/ oben.
COPY --from=frontend-builder /frontend-src/dist/radish-frontend/browser /frontend/dist/radish-frontend/browser

COPY docker/backend/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 8000

ENTRYPOINT ["/entrypoint.sh"]
