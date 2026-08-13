# Threads explizit, obwohl der Server selbst keinen Thread anlegt:
# zucchini_utils benutzt POSIX-Semaphore und Shared Memory, linkt in seinem
# eigenen CMakeLists.txt aber nicht dagegen -- das macht dort erst das Ziel
# zucchini_server. Ohne diese Zeile fehlen beim Linken die pthread-Symbole.
find_package(Threads REQUIRED)

add_executable(server
    src/main.c
    src/interface/command.c
)

# Fuer die eigenen Module des Servers, damit sie sich beim Namen ihres Moduls
# einbinden lassen und nicht ueber relative Pfade:
#
#     #include <interface/command.h>
#
# Bewusst src und nicht src/include wie beim Client: dessen Header tragen den
# radish/-Praefix, weil sie sich wie die der Bibliotheken lesen sollen. Die
# Module hier gehoeren allein zum Server und werden nirgends sonst eingebunden.
target_include_directories(server PRIVATE
    src
)

# radish_game ist der Spielzustand, zucchini_api die Verbindung nach draussen.
# Beide tragen ihre Include-Verzeichnisse PUBLIC, es braucht hier also keine
# weiteren Suchpfade.
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
