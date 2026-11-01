# game-server (radish/game-server/): Betriebs-Oberflaeche fuer die
# Game-Server-Instanzen auf diesem Host. Baut den C-Quellcode aus
# radish/game-server-core/ mit ein (zucchini_server + radish_server, siehe
# dort fuer die Begruendung der Shared-Memory-Kopplung): dieser Container
# startet Django und vier Game-Instanzen nebeneinander und haelt alle am
# Leben (siehe supervisord.conf) -- ein separater Einzel-Instanz-Container
# wie frueher gibt es deshalb nicht mehr.
#
# Vier Instanzen, game-instance-1 bis -4. Name und UDP-Port je Instanz
# stehen in supervisord.conf; die Ports (9999-10002) hier unter EXPOSE muessen
# dazu passen.
#
# Der Build-Kontext ist deshalb die Wurzel des Repositorys, nicht
# radish/game-server/ allein.
FROM alpine:3.20 AS build

# protobuf/protobuf-c-compiler fuer den Protobuf-Schritt (siehe
# radish/cmake/protobuf-c.cmake und radish/protobuf/CMakeLists.txt): protobuf
# liefert protoc, protobuf-c-compiler liefert protoc-gen-c -- beide sucht
# protobuf-c.cmake als Host-Werkzeuge (find_program) und bricht beim
# Konfigurieren sonst mit einer klaren Fehlermeldung ab. Das Alpine-Paket
# "protobuf-c" allein enthaelt nur die Laufzeitbibliothek, nicht das Plugin
# (und die Laufzeit wird ohnehin aus dem protobuf-c-Klon gebaut).
RUN apk add --no-cache build-base cmake protobuf protobuf-c-compiler

WORKDIR /src

COPY zucchini/ zucchini/
COPY jsmn/ jsmn/
# Wie jsmn ein blosser Klon und kein Paket: radish/cmake/protobuf-c.cmake
# uebersetzt die protobuf-c-Laufzeit (protobuf-c/protobuf-c.c) selbst aus ihm,
# statt sie zu verlinken -- siehe dort fuer die Begruendung.
COPY protobuf-c/ protobuf-c/
# Nur, was der Host-Build von radish braucht (radish/CMakeLists.txt ohne
# Emscripten: game/, protobuf/, game-server-core/ und cmake/) -- nicht ganz
# radish/. Sonst loeste jede Aenderung an Frontend, Backend oder den Bildern
# einen kompletten Neubau der C-Programme aus.
COPY radish/CMakeLists.txt radish/
COPY radish/cmake/ radish/cmake/
COPY radish/game/ radish/game/
COPY radish/protobuf/ radish/protobuf/
COPY radish/game-server-core/ radish/game-server-core/

# Erst Zucchini als eigenes Dachprojekt (zucchini_server + Admin-Client),
# dann radish ohne Emscripten (Bibliotheken + Server aus
# radish/game-server-core/, der Client wird uebersprungen).
RUN cmake -S zucchini -B build/zucchini \
        -DZUC_PLATFORM=linux \
        -DCMAKE_BUILD_TYPE=Release \
        -DZUC_BUILD_REMOTE_CLIENT_TEST=OFF \
    && cmake --build build/zucchini

RUN cmake -S radish -B build/radish -DCMAKE_BUILD_TYPE=Release \
        -DRAD_BUILD_TESTS=OFF \
    && cmake --build build/radish


# Zweite Stufe auf einem Python- statt einem reinen Alpine-Image: die
# Binaries oben sind gegen musl gelinkt, python:3.12-alpine ist ebenfalls
# Alpine-basiert und damit lauffaehig kompatibel. Kein Build-Stage-Umweg wie
# in docker/backend/Dockerfile noetig -- game-server braucht (noch) kein
# Paket mit nativen Extensions, siehe requirements.txt.
FROM python:3.12-alpine

# Im Ziel-Image heisst das Programm radish_server, im Quellbaum (unter
# radish/game-server-core/) bloss "server".
COPY --from=build /src/build/zucchini/server/zucchini_server             /usr/local/bin/zucchini_server
COPY --from=build /src/build/zucchini/admin_client/zucchini_admin_client /usr/local/bin/zucchini_admin_client
COPY --from=build /src/build/radish/game-server-core/server              /usr/local/bin/radish_server

# Die Welten, mit denen ein Spiel beginnen kann, und die, die radish_server ohne
# weitere Angabe laedt (RADISH_WORLD_PATH, siehe radish/game-server-core/src/
# main.c). Aus dem Quellbaum und nicht aus der Build-Stufe: es sind Daten, keine
# Build-Ergebnisse. Eine andere Welt laesst sich per Volume unter demselben Pfad
# oder ueber die Variable in docker-compose.yaml waehlen.
COPY radish/assets/worlds/ /usr/local/share/radish/worlds/
ENV RADISH_WORLD_PATH=/usr/local/share/radish/worlds/default.json

WORKDIR /app
# requirements.txt nur fuer die Installation, nicht ins Image.
COPY radish/game-server/requirements.txt /tmp/requirements.txt
RUN pip install --no-cache-dir -r /tmp/requirements.txt && rm /tmp/requirements.txt

# Nur, was zur Laufzeit gebraucht wird: die beiden Python-Pakete. gunicorn
# laedt config.wsgi direkt, manage.py braucht es im Image nicht. Bewusst nicht
# radish/game-server/ als Ganzes -- README, .gitignore und neue Dateien
# daneben sollen nicht ungefragt im Image landen. Tests, __pycache__/ und
# Editor-Reste in den Paketen haelt die .dockerignore fern.
COPY radish/game-server/config/ /app/config/
COPY radish/game-server/instances/ /app/instances/

# Produktionsmodus (siehe DEBUG/SECRET_KEY in config/settings.py): ohne
# DJANGO_SECRET_KEY aus der Umgebung startet Django nicht.
ENV DJANGO_DEBUG=0

# supervisord haelt Django und die Game-Instanz nebeneinander am Leben und
# startet neu, was abstuerzt (siehe supervisord.conf). entrypoint.sh bleibt
# der eigentliche Container-Einstiegspunkt (Migrationen zuerst, danach exec
# supervisord) -- game-instance-entrypoint.sh ist das Kommando, das
# supervisord fuer die Game-Instanz ausfuehrt (siehe dort fuer die
# Begruendung der Prozesspaarung).
COPY docker/game-server/entrypoint.sh /entrypoint.sh
COPY docker/game-server/game-instance-entrypoint.sh /usr/local/bin/game-instance-entrypoint.sh
COPY docker/game-server/register_instance.py /usr/local/bin/register_instance.py
COPY docker/game-server/supervisord.conf /etc/supervisord.conf
# 755 bzw. a+rX statt nur +x: COPY uebernimmt die Rechte aus dem
# Arbeitsverzeichnis, und dort sind manche Dateien nur fuer ihren Besitzer
# lesbar (0600/0711) -- fuer root egal, fuer den Benutzer radish unten nicht.
RUN chmod 755 /entrypoint.sh /usr/local/bin/game-instance-entrypoint.sh /usr/local/bin/register_instance.py \
    && chmod 644 /etc/supervisord.conf \
    && chmod -R a+rX /app

# Nicht als root, aber alle Prozesse unter demselben Benutzer (siehe
# supervisord.conf). Beschreibbar braucht radish nur zwei Verzeichnisse:
# /run/radish fuer die Dateien der Instanzen (RADISH_GAME_DATA_DIR, siehe
# game-instance-entrypoint.sh) und /run/supervisord fuer PID-Datei und Socket
# von supervisord. Die IPC von zucchini liegt unter /tmp und wird mit 0666
# angelegt (zucchini/utils/src/ipc/), das geht ohne Weiteres. Code und Programme
# gehoeren weiter root und sind fuer radish nur lesbar.
RUN addgroup -S radish && adduser -S -D -H -G radish radish \
    && mkdir -p /run/radish /run/supervisord \
    && chown radish:radish /run/radish /run/supervisord
USER radish

EXPOSE 8000
EXPOSE 9999-10002/udp

ENTRYPOINT ["/entrypoint.sh"]
