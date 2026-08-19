# Ein Testverzeichnis je Modul unter src/, und in jedem genau ein Programm. Der
# Spiegel ist Absicht: wer eine Quelle aendert, weiss ohne Suchen, wo ihre Tests
# stehen, und ein neues Modul bringt sein Testverzeichnis mit.
#
#     src/                    test/
#       game.c player.c   ->    game/              test_game
#       view.c            ->    game/
#       model/            ->    model/             test_model
#       control/command/  ->    control/command/   test_control_command
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
# (control_command), damit er eindeutig bleibt.
###
function(rad_add_game_test name)
    add_executable(test_${name} ${ARGN})

    target_link_libraries(test_${name} PRIVATE radish_game unity::framework)
    target_include_directories(test_${name} PRIVATE ${RAD_GAME_PRIVATE_INCLUDE})

    add_test(NAME ${name} COMMAND test_${name})
endfunction()

add_subdirectory(game)
add_subdirectory(model)
add_subdirectory(control/command)
add_subdirectory(control/events)
add_subdirectory(control/execute)
