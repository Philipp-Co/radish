# Tests des Clients, die auf dem Host laufen. Der Client selbst baut nur unter
# Emscripten (client/CMakeLists.txt); getestet wird hier, was ohne SDL
# auskommt -- die Leser der sprite.json (rendering/iso_unit_sprite.c), der
# tile.json (rendering/iso_tile_sprite.c) und der effect.json
# (rendering/iso_effect_sprite.c) und die Animationen der Figuren (rendering/animations/unit_animation.c). Die Quellen werden
# direkt uebersetzt, es gibt keine Bibliothek des Clients.

set(RAD_CLIENT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_executable(test_client
    main.c
    test_iso_unit_sprite.c
    test_iso_tile_sprite.c
    test_iso_effect_sprite.c
    test_unit_animation.c
    ${RAD_CLIENT_SOURCE}/view/rendering/iso_unit_sprite.c
    ${RAD_CLIENT_SOURCE}/view/rendering/iso_tile_sprite.c
    ${RAD_CLIENT_SOURCE}/view/rendering/iso_effect_sprite.c
    ${RAD_CLIENT_SOURCE}/view/rendering/iso_facing.c
    ${RAD_CLIENT_SOURCE}/view/rendering/animations/unit_animation.c
    ${RAD_CLIENT_SOURCE}/io/jsmn_impl.c
)
target_include_directories(test_client PRIVATE ${RAD_CLIENT_SOURCE}/include)
target_link_libraries(test_client PRIVATE unity::framework jsmn::jsmn m)
# Die Beispiele der Schema-Tests unten, darunter die sprite.json des schweren
# Panzers (valid/schwerer_panzer.json, wie sie das Backend aus seed.sql
# erzeugt): der Leser muss genau das annehmen und abweisen, was das Schema
# annimmt und abweist.
target_compile_definitions(test_client PRIVATE
    RAD_SPRITE_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/schema/sprite"
    RAD_TILE_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/schema/tile"
    RAD_EFFECT_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/schema/effect")
add_test(NAME client COMMAND test_client)

###
# Das Schema der sprite.json (client/schema/sprite.schema.json), wie die
# Schemas des Spiels mit check-jsonschema (game/test/CMakeLists.txt). Fehlt es,
# fehlen nur diese Tests. Gueltig muessen die Beispiele unter valid/ sein.
# Die sprite.json der Einheitenbilder selbst erzeugt das Backend aus der
# Datenbank; dort prueft sie api/tests/domain/sprites/test_sprites.py gegen
# dasselbe Schema.
###
find_program(RAD_CHECK_JSONSCHEMA check-jsonschema)

if(RAD_CHECK_JSONSCHEMA)
    set(RAD_SPRITE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/../schema/sprite.schema.json")

    add_test(NAME schema_sprite
        COMMAND ${RAD_CHECK_JSONSCHEMA} --check-metaschema ${RAD_SPRITE_SCHEMA})

    file(GLOB RAD_VALID_SPRITES "${CMAKE_CURRENT_SOURCE_DIR}/schema/sprite/valid/*.json")
    foreach(example ${RAD_VALID_SPRITES})
        get_filename_component(example_name ${example} NAME_WE)
        add_test(NAME schema_sprite_valid_${example_name}
            COMMAND ${RAD_CHECK_JSONSCHEMA} --schemafile ${RAD_SPRITE_SCHEMA} ${example})
    endforeach()

    file(GLOB RAD_INVALID_SPRITES "${CMAKE_CURRENT_SOURCE_DIR}/schema/sprite/invalid/*.json")
    foreach(example ${RAD_INVALID_SPRITES})
        get_filename_component(example_name ${example} NAME_WE)
        add_test(NAME schema_sprite_invalid_${example_name}
            COMMAND ${RAD_CHECK_JSONSCHEMA} --schemafile ${RAD_SPRITE_SCHEMA} ${example})
        set_tests_properties(schema_sprite_invalid_${example_name} PROPERTIES WILL_FAIL TRUE)
    endforeach()

    ###
    # Dasselbe fuer die tile.json (client/schema/tile.schema.json). Die Dateien
    # selbst erzeugt das Backend; dort prueft sie api/tests/domain/tiles/
    # test_tiles.py gegen dasselbe Schema.
    ###
    set(RAD_TILE_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/../schema/tile.schema.json")

    add_test(NAME schema_tile
        COMMAND ${RAD_CHECK_JSONSCHEMA} --check-metaschema ${RAD_TILE_SCHEMA})

    file(GLOB RAD_VALID_TILES "${CMAKE_CURRENT_SOURCE_DIR}/schema/tile/valid/*.json")
    foreach(example ${RAD_VALID_TILES})
        get_filename_component(example_name ${example} NAME_WE)
        add_test(NAME schema_tile_valid_${example_name}
            COMMAND ${RAD_CHECK_JSONSCHEMA} --schemafile ${RAD_TILE_SCHEMA} ${example})
    endforeach()

    file(GLOB RAD_INVALID_TILES "${CMAKE_CURRENT_SOURCE_DIR}/schema/tile/invalid/*.json")
    foreach(example ${RAD_INVALID_TILES})
        get_filename_component(example_name ${example} NAME_WE)
        add_test(NAME schema_tile_invalid_${example_name}
            COMMAND ${RAD_CHECK_JSONSCHEMA} --schemafile ${RAD_TILE_SCHEMA} ${example})
        set_tests_properties(schema_tile_invalid_${example_name} PROPERTIES WILL_FAIL TRUE)
    endforeach()

    ###
    # Dasselbe fuer die effect.json (client/schema/effect.schema.json). Die
    # Dateien selbst erzeugt das Backend; dort prueft sie api/tests/domain/
    # effects/test_effects.py gegen dasselbe Schema.
    ###
    set(RAD_EFFECT_SCHEMA "${CMAKE_CURRENT_SOURCE_DIR}/../schema/effect.schema.json")

    add_test(NAME schema_effect
        COMMAND ${RAD_CHECK_JSONSCHEMA} --check-metaschema ${RAD_EFFECT_SCHEMA})

    file(GLOB RAD_VALID_EFFECTS "${CMAKE_CURRENT_SOURCE_DIR}/schema/effect/valid/*.json")
    foreach(example ${RAD_VALID_EFFECTS})
        get_filename_component(example_name ${example} NAME_WE)
        add_test(NAME schema_effect_valid_${example_name}
            COMMAND ${RAD_CHECK_JSONSCHEMA} --schemafile ${RAD_EFFECT_SCHEMA} ${example})
    endforeach()

    file(GLOB RAD_INVALID_EFFECTS "${CMAKE_CURRENT_SOURCE_DIR}/schema/effect/invalid/*.json")
    foreach(example ${RAD_INVALID_EFFECTS})
        get_filename_component(example_name ${example} NAME_WE)
        add_test(NAME schema_effect_invalid_${example_name}
            COMMAND ${RAD_CHECK_JSONSCHEMA} --schemafile ${RAD_EFFECT_SCHEMA} ${example})
        set_tests_properties(schema_effect_invalid_${example_name} PROPERTIES WILL_FAIL TRUE)
    endforeach()
endif()
