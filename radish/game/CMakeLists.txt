# Ein Ziel fuer das ganze Spielmodul: die Spiellogik und der Codec der Kommandos.
#
# radish_game haengt an nichts -- kein SDL, kein Emscripten, kein Parser --, und
# deshalb steht hier auch kein target_link_libraries. Das ist der Grund, warum es
# im Browser wie auf dem Host baut.
#
# Vorher lag control/ in einem zweiten Ziel (radish_control): der Loader darin
# brauchte die Serialisierung, und weil die schon das Spiel kennt, waere daraus
# der Ring game -> serialization -> game geworden. Den Loader gibt es nicht mehr,
# control/ enthaelt nur noch command/, und das kommt mit stdint aus. Damit fiel
# der Grund fuer die Trennung weg.
add_library(radish_game STATIC
    src/game.c
    src/world.c
    src/player.c
    src/turn.c
    src/events/event_manager.c

    src/control/command/byte_writer.c
    src/control/command/byte_reader.c
    src/control/command/codec.c
    src/control/command/spawn_entity.c
    src/control/command/move_entity.c
    src/control/command/remove_entity.c
    src/control/command/create_tile.c
    src/control/command/remove_tile.c
    src/control/command/response.c
    
    src/control/execute/entity.c
)

target_include_directories(radish_game PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)
