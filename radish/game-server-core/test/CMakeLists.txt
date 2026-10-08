# Tests des Servers, nach demselben Muster wie game/test/: ein Verzeichnis je
# Modul unter src/, darin genau ein Programm.
#
#     src/                    test/
#       control/          ->    control/            test_server_control
#       interface/        ->    interface/          test_server_interface
#
# Der Server ist ein Programm und keine Bibliothek -- es gibt kein Ziel, gegen
# das sich linken liesse. Ein Test nimmt deshalb die Quellen, die er prueft,
# selbst mit, und sieht das Include-Verzeichnis des Servers (src/include/).
# Das ist vor allem control/execute.c, das das Spiel steuert -- zusammen mit
# radish_game, das auch die Spielstart-Datei liest (radish/game/control/
# start_game.h); deren Tests liegen deshalb unter game/test/control/start_game/.

set(RAD_SERVER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_executable(test_server_control
    control/main.c
    control/test_start_game.c
    control/test_deploy.c
    ${RAD_SERVER_SOURCE_DIR}/control/execute.c
    ${RAD_SERVER_SOURCE_DIR}/control/loader.c
)
target_include_directories(test_server_control PRIVATE ${RAD_SERVER_SOURCE_DIR}/include)
target_link_libraries(test_server_control PRIVATE radish_game unity::framework)

# Die Beispiele der Schema-Tests, fuer den Test, der den Spielstart aus der Datei
# einrichtet -- dieselbe Datei, die check-jsonschema prueft (game/test/CMakeLists.txt).
target_compile_definitions(test_server_control PRIVATE
    RAD_SCHEMA_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../game/test/schema")

add_test(NAME server_control COMMAND test_server_control)

# Die Aussengrenze: message.c gegen den generierten Code aus protobuf/*.proto. Der
# Test baut die Nachrichten selbst und sieht dafuer radish_protobuf -- message.h
# zeigt die generierten Typen nach aussen nicht.
add_executable(test_server_interface
    interface/main.c
    interface/test_message.c
    ${RAD_SERVER_SOURCE_DIR}/interface/message.c
)
target_include_directories(test_server_interface PRIVATE ${RAD_SERVER_SOURCE_DIR}/include)
target_link_libraries(test_server_interface PRIVATE radish_game radish_protobuf unity::framework)

add_test(NAME server_interface COMMAND test_server_interface)
