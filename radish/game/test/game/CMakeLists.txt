rad_add_game_test(game main.c test_game.c test_move_command.c test_attack_command.c test_view.c test_tile.c test_world_definition.c test_add_unit.c test_deploy_command.c test_players.c test_turn_events.c)

# Die Welt aus dem Repository, fuer den Test, der wirklich eine Datei liest
# (test_world_definition.c).
target_compile_definitions(test_game PRIVATE RAD_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../../assets")
