# Ein Ziel fuer das ganze Spielmodul: die Spiellogik und der Codec der Kommandos.
#
# radish_game haengt an nichts -- kein SDL, kein Emscripten, kein Parser --, und
# deshalb steht hier auch kein target_link_libraries. Das ist der Grund, warum es
# im Browser wie auf dem Host baut.
#
# Vorher lag control/ in einem zweiten Ziel (radish_control): der Loader darin
# brauchte die Serialisierung, und weil die schon das Spiel kennt, waere daraus
# der Ring game -> serialization -> game geworden. Den Loader gibt es nicht mehr,
# unter control/ liegen nur noch command/, events/ und execute/, und die kommen
# mit stdint aus. Damit fiel der Grund fuer die Trennung weg.
add_library(radish_game STATIC
    src/game.c
    src/player.c
    src/view.c
    src/tile.c

    src/model/world.c
    src/model/turn.c

    src/control/command/byte_writer.c
    src/control/command/byte_reader.c
    src/control/command/codec.c
    src/control/command/spawn_entity.c
    src/control/command/move_entity.c
    src/control/command/remove_entity.c
    src/control/command/create_tile.c
    src/control/command/remove_tile.c
    src/control/command/shoot.c
    src/control/command/use.c
    src/control/command/response.c
    
    src/control/events/event_manager.c
    src/control/execute/entity.c
)

# Zwei Suchpfade, und der Unterschied ist die Grenze des Moduls: unter include/
# liegt die Schnittstelle -- game.h, das Vokabular des Modells (model.h), die
# Kommandos und die Ereignisse. Unter src/include/ liegt der Spielzustand selbst.
# Er geht PRIVATE nach niemandem heraus: wer die Strukturen nicht hat, kann an den
# Regeln nicht vorbei in der Welt schreiben.
#
# Die Grenze verlaeuft dabei durch model/ hindurch und nicht darum herum: Tile und
# Entitaet liegen mit ihren Feldern unter include/, weil die Ereignisse sie nach
# draussen reichen und ein Abonnent in ihnen lesen muss. Welt, Zug und Spiel liegen
# unter src/include/ und bleiben nach aussen bloss Namen -- sie sind das, was das
# Modul fuehrt, nicht das, was es erzaehlt. Warum das keine halbe Kapselung ist,
# steht in include/radish/game/model.h.
#
# Dieselbe Aufteilung wie beim Server und beim Client, wo include/ ebenfalls
# innerhalb von src/ liegt -- nur dass dort alles privat ist, weil sie Programme
# sind und keine Bibliothek.
target_include_directories(radish_game
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)
