# Ein Testverzeichnis je Modul unter src/, und in jedem genau ein Programm. Der
# Spiegel ist Absicht: wer eine Quelle aendert, weiss ohne Suchen, wo ihre Tests
# stehen, und ein neues Modul bringt sein Testverzeichnis mit.
#
#     src/                    test/
#       game.c player.c   ->    game/              test_game
#       view.c            ->    game/
#       model/            ->    model/             test_model
#       control/events/   ->    control/events/    test_control_events
#       control/execute/  ->    control/execute/   test_control_execute

###
# Die Tests sehen den privaten Teil des Moduls.
#
# radish_game traegt src/include/ als PRIVATE -- von aussen ist der Spielzustand
# nicht zu sehen, und das ist der Sinn der Sache. Ein Test ist aber kein Aufrufer
# von aussen: er gehoert zum Modul. Ohne diesen Pfad liesse sich die halbe
# Spiellogik gar nicht pruefen, weil RAD_Turn_t und RAD_World_t unvollstaendig
# blieben; mit ihm laesst sich pruefen, was eine Funktion wirklich hinschreibt,
# und nicht nur, was die Fassade davon durchreicht.
###
set(RAD_GAME_PRIVATE_INCLUDE "${CMAKE_CURRENT_SOURCE_DIR}/../src/include")

###
# Legt ein Testprogramm an und meldet es bei ctest an.
#
#     rad_add_game_test(model main.c test_turn.c test_world.c)
#
# Der Name ist zugleich der des Tests in ctest und, mit test_ davor, der des
# Programms. Bei geschachtelten Modulen wird er zusammengezogen
# (control_events, control_execute), damit er eindeutig bleibt.
###
function(rad_add_game_test name)
    add_executable(test_${name} ${ARGN})

    target_link_libraries(test_${name} PRIVATE radish_game unity::framework)
    target_include_directories(test_${name} PRIVATE ${RAD_GAME_PRIVATE_INCLUDE})

    # Aus demselben Grund jsmn: die privaten Leser (json_reader.h und wer ihn
    # einbindet) nennen jsmntok_t, und radish_game haengt jsmn nur PRIVATE an.
    target_link_libraries(test_${name} PRIVATE jsmn::jsmn)

    add_test(NAME ${name} COMMAND test_${name})
endfunction()

add_subdirectory(game)
add_subdirectory(model)
add_subdirectory(control/events)
add_subdirectory(control/execute)

###
# Das Schema der Weltdefinition und die Welten, die danach geschrieben sind.
#
# Geprueft wird mit check-jsonschema, einem Python-Werkzeug ausserhalb des
# Builds (pipx install check-jsonschema). Fehlt es, fehlen nur diese Tests --
# der Build haengt nicht daran, er liest das Schema nie.
#
# Drei Arten Test: das Schema selbst gegen das Metaschema, jede Welt unter
# assets/worlds/ als gueltig, und jede Datei unter schema/invalid/ als ungueltig
# (WILL_FAIL). Die letzten halten das Schema streng: wird es aus Versehen
# nachgiebig, faellt ein Test um, statt dass eine kaputte Welt durchgeht.
###
find_program(RAD_CHECK_JSONSCHEMA check-jsonschema)

if(RAD_CHECK_JSONSCHEMA)
    set(RAD_WORLD_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/../schema/world.schema.json")

    add_test(NAME schema_world
        COMMAND ${RAD_CHECK_JSONSCHEMA} --check-metaschema ${RAD_WORLD_SCHEMA})

    file(GLOB RAD_VALID_WORLDS "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/worlds/*.json")
    foreach(world ${RAD_VALID_WORLDS})
        get_filename_component(world_name ${world} NAME_WE)
        add_test(NAME schema_world_valid_${world_name}
            COMMAND ${RAD_CHECK_JSONSCHEMA} --schemafile ${RAD_WORLD_SCHEMA} ${world})
    endforeach()

    file(GLOB RAD_INVALID_WORLDS "${CMAKE_CURRENT_SOURCE_DIR}/schema/invalid/*.json")
    foreach(world ${RAD_INVALID_WORLDS})
        get_filename_component(world_name ${world} NAME_WE)
        add_test(NAME schema_world_invalid_${world_name}
            COMMAND ${RAD_CHECK_JSONSCHEMA} --schemafile ${RAD_WORLD_SCHEMA} ${world})
        set_tests_properties(schema_world_invalid_${world_name} PROPERTIES WILL_FAIL TRUE)
    endforeach()
else()
    message(STATUS "check-jsonschema nicht gefunden -- Schema-Tests der Weltdefinition entfallen")
endif()
