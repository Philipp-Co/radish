# Ein Ziel fuer das ganze Spielmodul: die Spiellogik, der Codec der Kommandos und
# das Einlesen der Weltdefinition.
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
# Aus demselben Ring heraus liegt auch serialization/ hier und nicht in einer
# eigenen Bibliothek: der Leser der Weltdefinition schreibt in die Welt, braucht
# dafuer also ihre Struktur -- und die steht seit der Kapselung des Modells hinter
# src/include/. Eine Bibliothek daneben haette diesen Pfad von aussen gebraucht, und
# damit waere die Grenze fuer alle offen gewesen. Der Leser ist aber kein Aufrufer
# von aussen, sondern die Gegenseite zum Modell. Deshalb gehoert er hinein --
# dasselbe Argument, mit dem die Tests den privaten Pfad sehen (test/CMakeLists.txt).
add_library(radish_game STATIC
    src/game.c
    src/player.c
    src/view.c
    src/tile.c

    src/model/world.c
    src/model/turn.c

    src/control/events/event_manager.c
    src/control/execute/unit.c

    src/serialization/jsmn_impl.c
    src/serialization/json_reader.c
    src/serialization/serialization.c
    src/serialization/world_file.c
    src/serialization/world_definition.c
)

# Zwei Suchpfade, und der Unterschied ist die Grenze des Moduls: unter include/
# liegt die Schnittstelle -- game.h, das Vokabular des Modells (model.h), die
# Kommandos und die Ereignisse. Unter src/include/ liegt der Spielzustand selbst.
# Er geht PRIVATE nach niemandem heraus: wer die Strukturen nicht hat, kann an den
# Regeln nicht vorbei in der Welt schreiben.
#
# Die Grenze verlaeuft dabei durch model/ hindurch und nicht darum herum: Tile und
# Einheit liegen mit ihren Feldern unter include/, weil die Ereignisse sie nach
# draussen reichen und ein Abonnent in ihnen lesen muss. Welt, Zug und Spiel liegen
# unter src/include/ und bleiben nach aussen bloss Namen -- sie sind das, was das
# Modul fuehrt, nicht das, was es erzaehlt. Warum das keine halbe Kapselung ist,
# steht in include/radish/game/model.h.
#
# serialization/ liegt dagegen vollstaendig privat, jeder ihrer drei Header --
# das Ergebnis, der JSON-Leser und die Weltdefinition. Nach aussen geht von ihr
# keine Datei, sondern drei Zeilen in game.h: laden, das Ergebnis und sein Text
# (RAD_LoadWorldFromFile, RAD_GameLoadResultText).
#
# Das ist der Zweck der Uebung. Ein Aufrufer soll eine Weltdefinition nennen
# koennen, ohne das Format zu kennen, ohne einen Puffer zu stellen und ohne zu
# wissen, dass JSON im Spiel ist. Was eine Weltdefinition ist, entscheidet nur
# dieses Modul.
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
