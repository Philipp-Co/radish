# Tests des Clients, die auf dem Host laufen. Der Client selbst baut nur unter
# Emscripten (client/CMakeLists.txt); getestet wird hier, was ohne SDL
# auskommt -- bisher der Leser der sprite.json (rendering/iso_unit_sprite.c).
# Die Quellen werden direkt uebersetzt, es gibt keine Bibliothek des Clients.

set(RAD_CLIENT_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../src")

add_executable(test_client_sprite
    main.c
    test_iso_unit_sprite.c
    ${RAD_CLIENT_SOURCE}/rendering/iso_unit_sprite.c
    ${RAD_CLIENT_SOURCE}/io/jsmn_impl.c
)
target_include_directories(test_client_sprite PRIVATE ${RAD_CLIENT_SOURCE}/include)
target_link_libraries(test_client_sprite PRIVATE unity::framework jsmn::jsmn m)
# Die Beispiele der Schema-Tests unten und das Bild des Panzers: der Leser muss
# genau das annehmen und abweisen, was das Schema annimmt und abweist.
target_compile_definitions(test_client_sprite PRIVATE
    RAD_SPRITE_EXAMPLES_DIR="${CMAKE_CURRENT_SOURCE_DIR}/schema/sprite"
    RAD_ASSETS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/../../assets")
add_test(NAME client_sprite COMMAND test_client_sprite)

###
# Das Schema der sprite.json (client/schema/sprite.schema.json), wie die
# Schemas des Spiels mit check-jsonschema (game/test/CMakeLists.txt). Fehlt es,
# fehlen nur diese Tests. Gueltig muessen die Beispiele unter valid/ sein und
# jede sprite.json unter assets/units/ -- eine kaputte Grafik faellt so schon
# hier auf und nicht erst im Browser.
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

    file(GLOB RAD_ASSET_SPRITES "${CMAKE_CURRENT_SOURCE_DIR}/../../assets/units/*/sprite.json")
    foreach(sprite ${RAD_ASSET_SPRITES})
        get_filename_component(sprite_dir ${sprite} DIRECTORY)
        get_filename_component(sprite_key ${sprite_dir} NAME)
        add_test(NAME schema_sprite_asset_${sprite_key}
            COMMAND ${RAD_CHECK_JSONSCHEMA} --schemafile ${RAD_SPRITE_SCHEMA} ${sprite})
    endforeach()

    file(GLOB RAD_INVALID_SPRITES "${CMAKE_CURRENT_SOURCE_DIR}/schema/sprite/invalid/*.json")
    foreach(example ${RAD_INVALID_SPRITES})
        get_filename_component(example_name ${example} NAME_WE)
        add_test(NAME schema_sprite_invalid_${example_name}
            COMMAND ${RAD_CHECK_JSONSCHEMA} --schemafile ${RAD_SPRITE_SCHEMA} ${example})
        set_tests_properties(schema_sprite_invalid_${example_name} PROPERTIES WILL_FAIL TRUE)
    endforeach()
endif()
