# Ein Ziel fuer das ganze Spielmodul: die Spiellogik, der Codec der Kommandos und
# die Serialisierung des Spielstands.
#
# radish_game haengt an nichts, was eine Umgebung mitbringt -- kein SDL, kein
# Emscripten, kein POSIX. Das ist der Grund, warum es im Browser wie auf dem Host
# baut. Die eine Abhaengigkeit ist jsmn, ein Header-only-Parser ohne eigene
# Plattformannahmen; er geht PRIVATE nach niemandem heraus (siehe unten).
#
# Vorher lag control/ in einem zweiten Ziel (radish_control): der Loader darin
# brauchte die Serialisierung, und weil die schon das Spiel kennt, waere daraus
# der Ring game -> serialization -> game geworden. Den Loader gibt es nicht mehr,
# unter control/ liegen nur noch command/, events/ und execute/, und die kommen
# mit stdint aus. Damit fiel der Grund fuer die Trennung weg.
#
# Aus demselben Ring heraus liegt inzwischen auch serialization/ hier und nicht
# mehr in einer eigenen Bibliothek: die Serialisierer bilden jedes Feld von Welt
# und Spiel ab, brauchen dafuer also deren Strukturen -- und die stehen seit der
# Kapselung des Modells hinter src/include/. Eine Bibliothek daneben haette diesen
# Pfad von aussen gebraucht, und damit waere die Grenze fuer alle offen gewesen.
# Ein Serialisierer ist aber kein Aufrufer von aussen, sondern die Gegenseite zum
# Modell: er schreibt auf, was das Modul fuehrt. Deshalb gehoert er hinein --
# dasselbe Argument, mit dem die Tests den privaten Pfad sehen (test/CMakeLists.txt).
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

    src/serialization/jsmn_impl.c
    src/serialization/json_writer.c
    src/serialization/json_reader.c
    src/serialization/tile_serializer.c
    src/serialization/entity_serializer.c
    src/serialization/world_serializer.c
    src/serialization/game_serializer.c
    src/serialization/serialization.c
    src/serialization/save_file.c
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
# serialization/ liegt dagegen vollstaendig privat, jeder ihrer sieben Header --
# das Format, der JSON-Leser, der Schreiber und die vier Serialisierer. Nach aussen
# geht von ihr keine Datei, sondern vier Zeilen in game.h: speichern, laden, das
# Ergebnis und sein Text (RAD_SaveGameToFile, RAD_LoadGameFromFile).
#
# Das ist der Zweck der Uebung. Ein Aufrufer soll einen Spielstand nennen koennen,
# ohne das Format zu kennen, ohne einen Puffer zu stellen und ohne zu wissen, dass
# JSON im Spiel ist -- vorher lag beides offen, und der Server trug die Haelfte der
# Arbeit selbst (sein control/loader/save_file.c gibt es deshalb nicht mehr).
# Getauscht wird damit eine breite Schnittstelle gegen eine schmale: was ein
# Spielstand ist, entscheidet ab jetzt nur dieses Modul.
#
# Dieselbe Aufteilung wie beim Server und beim Client, wo include/ ebenfalls
# innerhalb von src/ liegt -- nur dass dort alles privat ist, weil sie Programme
# sind und keine Bibliothek.
target_include_directories(radish_game
    PUBLIC  ${CMAKE_CURRENT_SOURCE_DIR}/include
    PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/include
)

# PRIVATE, und das ist die Probe darauf, dass die Aufteilung oben stimmt: jsmn.h
# kommt nur noch in json_reader.h vor, und der liegt hinter src/include/. Wer
# radish_game benutzt, bekommt den Parser damit nicht mehr auf den Suchpfad --
# vorher trug radish_serialization ihn PUBLIC weiter, weil sein oeffentlicher
# json_reader.h jsmntok_t im Interface fuehrte.
target_link_libraries(radish_game PRIVATE
    jsmn::jsmn
)
