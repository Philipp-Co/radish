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
    src/control/game_start.c
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
# jsmn dagegen doch, fuer eine Datei, die nicht dem Spiel gehoert: die
# Spielstart-Datei (control/game_start.c), in der das Backend ablegt, wer mit welcher
# Armee antritt. Woher die Spieler kommen, ist Sache des Servers -- er liest sie und
# richtet sein Spiel danach ueber dessen Setter ein. Eine eigene, statische Kopie des
# Parsers (JSMN_STATIC), damit sie nicht mit der in radish_game zusammenstoesst.
target_link_libraries(server PRIVATE
    radish_game
    radish_protobuf
    zucchini_api
    jsmn::jsmn
    Threads::Threads
)

# Kein RUNTIME_OUTPUT_DIRECTORY: anders als der Client, der als client.js/.wasm
# neben die index.html in web/ muss, bleibt der Server im Build-Verzeichnis.
set_target_properties(server PROPERTIES
    OUTPUT_NAME "server"
)
