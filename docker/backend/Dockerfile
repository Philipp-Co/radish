# Zucchini und der Spielserver in einem Image -- und in einem Container: sie
# reden nicht ueber das Netz, sondern ueber zwei Ringpuffer im Shared Memory und
# eine FIFO zum Aufwecken (siehe zucchini/api/api.h). Beides ist lokal, beide
# Prozesse muessen sich also denselben IPC-Namensraum und dasselbe /tmp teilen.
#
# Der Build-Kontext ist die Wurzel des Repositorys, weil drei Verzeichnisse
# hineingehoeren: radish/ selbst und die beiden Klone jsmn/ und zucchini/ daneben
# -- radishs CMake sucht sie genau dort. Was nicht mit hineingehoert, steht in
# .dockerignore; ohne die waere emsdk/ mit im Kontext.
FROM alpine:3.20 AS build

RUN apk add --no-cache build-base cmake

WORKDIR /src

COPY zucchini/ zucchini/
COPY jsmn/ jsmn/
COPY radish/ radish/

# Zucchini zuerst, als eigenes Dachprojekt: daraus kommen zucchini_server und der
# Admin-Client, mit dem der Einstiegspunkt spaeter die Whitelist setzt.
# ZUC_BUILD_REMOTE_CLIENT_TEST=OFF wie in zucchinis eigenem Test-Image -- die
# Testwerkzeuge braucht hier niemand.
RUN cmake -S zucchini -B build/zucchini \
        -DZUC_PLATFORM=linux \
        -DCMAKE_BUILD_TYPE=Release \
        -DZUC_BUILD_REMOTE_CLIENT_TEST=OFF \
    && cmake --build build/zucchini

# Dann radish ohne Emscripten -- das baut die Bibliotheken und den Server, der
# Client wird uebersprungen. zucchini_utils und zucchini_api uebersetzt dieser
# Build sich selbst aus dem Klon nebenan (radish/cmake/zucchini.cmake); deshalb
# liegt zucchini/ hier als Quelltext im Bild und nicht bloss als Binary.
RUN cmake -S radish -B build/radish -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build/radish


FROM alpine:3.20

# Im Ziel-Image heisst das Programm radish_server, im Quellbaum bloss "server".
# Neben zucchini_server waere "server" nicht mehr eindeutig.
COPY --from=build /src/build/zucchini/server/zucchini_server             /usr/local/bin/zucchini_server
COPY --from=build /src/build/zucchini/admin_client/zucchini_admin_client /usr/local/bin/zucchini_admin_client
COPY --from=build /src/build/radish/server/server                       /usr/local/bin/radish_server

COPY docker/backend/entrypoint.sh /entrypoint.sh
RUN chmod +x /entrypoint.sh

EXPOSE 9999/udp

ENTRYPOINT ["/entrypoint.sh"]
