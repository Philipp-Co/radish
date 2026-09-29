# game-server (radish/game-server/): Betriebs-Oberflaeche fuer die
# Game-Server-Instanzen auf diesem Host. Baut den C-Quellcode aus
# radish/game-server-core/ mit ein (zucchini_server + radish_server, siehe
# dort fuer die Begruendung der Shared-Memory-Kopplung): dieser Container
# startet Django und eine Game-Instanz nebeneinander und haelt beide am
# Leben (siehe supervisord.conf) -- ein separater Einzel-Instanz-Container
# wie frueher gibt es deshalb nicht mehr.
#
# Aktuell nur eine Instanz, nicht vier: zucchini_server hat Instanzname und
# UDP-Port fest einkompiliert (siehe Kommentar in supervisord.conf) und kann
# deshalb noch nicht mehrfach im selben Container laufen.
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
COPY radish/ radish/

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
COPY radish/game-server/requirements.txt /app/requirements.txt
RUN pip install --no-cache-dir -r requirements.txt

COPY radish/game-server/ /app/

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
RUN chmod +x /entrypoint.sh /usr/local/bin/game-instance-entrypoint.sh /usr/local/bin/register_instance.py

EXPOSE 8000
EXPOSE 9999/udp

ENTRYPOINT ["/entrypoint.sh"]
