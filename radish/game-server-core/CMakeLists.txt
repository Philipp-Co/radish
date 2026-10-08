# Threads explizit, obwohl der Server selbst keinen Thread anlegt:
# zucchini_utils benutzt POSIX-Semaphore und Shared Memory, linkt in seinem
# eigenen CMakeLists.txt aber nicht dagegen -- das macht dort erst das Ziel
# zucchini_server. Ohne diese Zeile fehlen beim Linken die pthread-Symbole.
find_package(Threads REQUIRED)

add_executable(server
    src/main.c
    src/interface/message.c
    src/control/events/tiles.c
    src/control/execute.c
    src/control/loader.c
)

# Fuer die eigenen Module des Servers, damit sie sich beim Namen ihres Moduls
# einbinden lassen und nicht ueber relative Pfade:
#
#     #include <radish/server/interface/message.h>
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

# radish_game ist der Spielzustand, radish_protobuf das Ein-/Auspacken der
# Nachrichten, zucchini_api die Verbindung nach draussen. Alle drei tragen
# ihre Include-Verzeichnisse PUBLIC, es braucht hier also keine weiteren
# Suchpfade.
#
# Das Einlesen der Weltdefinition steckt in radish_game und ist deshalb keine
# eigene Zeile. Der Server sieht davon nur RAD_LoadWorldFromFile aus game.h: kein
# Format, kein Puffer, kein jsmn -- das liegt alles hinter dem privaten Suchpfad
# des Spielmoduls.
#
# Dasselbe fuer die Spielstart-Datei, in der das Backend ablegt, wer mit welcher
# Armee antritt: sie liest radish_game (control/start_game.h). Frueher lag der
# Leser hier und brachte eine eigene Kopie von jsmn mit; die Zeile dafuer ist
# mit ihm gegangen.
target_link_libraries(server PRIVATE
    radish_game
    radish_protobuf
    zucchini_api
    Threads::Threads
)

# Kein RUNTIME_OUTPUT_DIRECTORY: anders als der Client, der als client.js/.wasm
# neben die index.html in web/ muss, bleibt der Server im Build-Verzeichnis.
set_target_properties(server PROPERTIES
    OUTPUT_NAME "server"
)
