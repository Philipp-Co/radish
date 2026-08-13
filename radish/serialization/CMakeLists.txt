add_library(radish_serialization STATIC
    src/jsmn_impl.c
    src/json_writer.c
    src/json_reader.c
    src/tile_serializer.c
    src/entity_serializer.c
    src/world_serializer.c
    src/game_serializer.c
    src/serialization.c
)

target_include_directories(radish_serialization PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# Beides PUBLIC: serialization.h bindet <radish/game/game.h> ein und
# json_reader.h fuehrt jsmntok_t im Interface. Wer die Bibliothek benutzt,
# braucht deshalb beide Header-Pfade.
target_link_libraries(radish_serialization PUBLIC
    radish_game
    jsmn::jsmn
)
