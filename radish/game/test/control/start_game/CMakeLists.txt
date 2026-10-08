rad_add_game_test(control_start_game main.c test_parse.c test_start_game.c)

# Die Beispiele der Schema-Tests, fuer die Tests, die die gueltige Spielstart-Datei
# lesen -- dieselbe Datei, die check-jsonschema prueft (game/test/CMakeLists.txt).
target_compile_definitions(test_control_start_game PRIVATE
    RAD_SCHEMA_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../schema")
