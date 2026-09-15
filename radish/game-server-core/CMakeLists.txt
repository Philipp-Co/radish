# Threads explizit, obwohl der Server selbst keinen Thread anlegt:
# zucchini_utils benutzt POSIX-Semaphore und Shared Memory, linkt in seinem
# eigenen CMakeLists.txt aber nicht dagegen -- das macht dort erst das Ziel
# zucchini_server. Ohne diese Zeile fehlen beim Linken die pthread-Symbole.
find_package(Threads REQUIRED)

add_executable(server
    src/main.c
    src/interface/command.c
    src/control/events/tiles.c
    src/control/execute.c
    src/control/loader.c
)

# Fuer die eigenen Module des Servers, damit sie sich beim Namen ihres Moduls
# einbinden lassen und nicht ueber relative Pfade:
#
#     #include <radish/server/interface/command.h>
#
# Dieselbe Form wie bei den Bibliotheken und beim Client: die oeffentlichen
# Header liegen unter include/radish/<projekt>/ und werden ueberall gleich
# eingebunden, egal von wo. Dass sie nur der Server benutzt, aendert daran
# nichts -- ein Include soll sich lesen lassen, ohne zu wissen, wo die Datei
# steht, und die Trennung Header/Quelle steht damit auch im Verzeichnisbaum.
#
# Wie beim Client liegt include/ innerhalb von src/, nicht daneben wie bei
# game/: der Server ist ein Programm und keine Bibliothek, sein
# Include-Verzeichnis geht deshalb PRIVATE und nicht PUBLIC nach draussen.
target_include_directories(server PRIVATE
    src/include
)

# radish_game ist der Spielzustand, zucchini_api die Verbindung nach draussen.
# Beide tragen ihre Include-Verzeichnisse PUBLIC, es braucht hier also keine
# weiteren Suchpfade.
#
# Die Serialisierung steckt in radish_game und ist deshalb keine eigene Zeile mehr.
# Der Server sieht von ihr nur noch RAD_LoadGameFromFile aus game.h: kein Format,
# kein Puffer, kein jsmn -- das liegt alles hinter dem privaten Suchpfad des
# Spielmoduls. Sein eigenes control/loader/save_file.c, das die Datei bisher selbst
# aufmachte, ist damit weggefallen.
target_link_libraries(server PRIVATE
    radish_game
    zucchini_api
    Threads::Threads
)

# Kein RUNTIME_OUTPUT_DIRECTORY: anders als der Client, der als client.js/.wasm
# neben die index.html in web/ muss, bleibt der Server im Build-Verzeichnis.
set_target_properties(server PROPERTIES
    OUTPUT_NAME "server"
)
