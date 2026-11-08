# emcc fuegt den Header-Suchpfad fuer per USE_SDL=2/USE_SDL_IMAGE=2 gezogene Ports
# nur intern beim eigentlichen Aufruf hinzu -- er taucht nicht in den Flags auf,
# die CMake in compile_commands.json schreibt. Fuer clangd/vim muss er deshalb
# hier explizit ergaenzt werden.
#
# EMSCRIPTEN_SYSROOT setzt die Toolchain-Datei bereits selbst; sie ruft em-config
# ueber den absoluten emsdk-Pfad auf. Ein eigener "em-config"-Aufruf wuerde
# dagegen voraussetzen, dass emsdk_env.sh den PATH gesetzt hat -- ohne das bleibt
# die Ausgabe leer und in compile_commands.json landet ein nutzloses
# "-I/sysroot/include", woraufhin clangd die SDL2-Header nicht findet.
if(NOT EMSCRIPTEN_SYSROOT)
    message(FATAL_ERROR "EMSCRIPTEN_SYSROOT ist nicht gesetzt -- wurde wirklich mit der Emscripten-Toolchain konfiguriert?")
endif()

add_executable(client
    src/main.c
    src/rendering/iso_object.c
    src/rendering/iso_map.c
    src/rendering/entity.c
    src/rendering/iso_tile_images.c
    src/rendering/iso_facing.c
    src/rendering/iso_unit_images.c
    src/rendering/iso_effect_images.c
    src/rendering/iso_effect.c
    src/rendering/bitmap_font.c
    src/rendering/iso_tile/iso_tile.c
    src/rendering/iso_tile/void.c
    src/rendering/iso_tile/ground.c
    src/rendering/iso_tile/water_shallow.c
    src/rendering/iso_tile/mud.c
    src/rendering/iso_tile/meadow.c
    src/rendering/iso_tile/forest.c
    src/rendering/iso_tile/water_deep.c
    src/rendering/iso_tile/city.c
    src/rendering/iso_tile/road.c
    src/rendering/iso_tile/fortification.c
    src/rendering/iso_tile/dragons_teeth.c
    src/rendering/iso_tile/high_grass.c
    src/rendering/iso_tile/unknown.c
    
    src/io/net_codec.c
    src/io/net_inbox.c
    src/io/camera_control.c
    src/io/net_transport.c

    src/io/net_event_handler/common.c
    src/io/net_event_handler/command_response.c
    src/io/net_event_handler/deploy_response.c
    src/io/net_event_handler/game_created.c
    src/io/net_event_handler/game_finished.c
    src/io/net_event_handler/round.c
    src/io/net_event_handler/current_player.c
    src/io/net_event_handler/players.c
    src/io/net_event_handler/world_size.c
    src/io/net_event_handler/deploy_zone.c
    src/io/net_event_handler/tiles.c
    src/io/net_event_handler/reserve_unit.c
    src/io/net_event_handler/unit_deployed.c
    src/io/net_event_handler/unit_moved.c
    src/io/net_event_handler/unit_attacked.c
    src/io/net_event_handler/unit_destroyed.c
    src/io/net_event_handler/unit_sighted.c
    src/io/net_event_handler/unit_lost.c
    src/io/net_event_handler/unit.c
    src/io/net_event_handler/end_turn_response.c
    src/io/net_event_handler/attack_response.c
    src/io/net_event_handler/tile_created.c
    src/io/net_event_handler/tile_removed.c
    src/io/net_event_handler/tile_changed.c

    src/io/net_request/move.c
    src/io/net_request/deploy.c
    src/io/net_request/end_turn.c
    src/io/net_request/attack.c
    src/io/net_request/discover.c
    src/io/net_request/discover_reserve.c
    src/io/net_request/discover_units.c

    src/events/event_manager.c

    src/control/terrain.c
    src/control/line_of_sight.c
    src/control/visibility.c
    src/control/deploy_unit.c
    src/control/move_unit.c
    src/control/attack_unit.c
    src/control/end_turn.c

    src/model/world.c
    src/model/unit.c
    src/model/unit_repository.c
    src/model/tile.c
    src/model/reserve.c
    src/model/events/observable.c

    src/view/view.c
    src/view/unit_info_view.c
    src/view/iso_map_view.c
    src/view/text_view.c
    src/view/typewriter_view.c
    src/view/context_menu/context_menu_view.c
    src/view/context_menu/context_menu_item.c
    src/view/context_menu/context_menu_state_machine.c
    src/view/unit_deployment_view.c
    src/view/debug/debug_view.c
    src/view/end_turn_view.c
    src/view/animations/interpolation.c
    src/view/animations/unit_animation.c
)

target_include_directories(client PRIVATE
    src/include
)

# Kein radish_game: der Client kennt das Spielmodul nicht. Was er von der Welt
# weiss, kommt ueber die Verbindung und steht in eigenen Typen
# (src/include/radish/io/net_types.h, src/model/world.c); die Schnittstelle zum
# Server ist allein protobuf/*.proto.
#
# radish_protobuf traegt den generierten Protobuf-Code (protobuf/CMakeLists.txt)
# und bringt ueber protobuf-c::protobuf-c auch dessen Header/Bibliothek mit --
# das braucht ausschliesslich src/io/net_codec.c, siehe dort.
target_link_libraries(client PRIVATE
    radish_protobuf
)

# CMake erkennt "${EMSCRIPTEN_CACHE_DIR}/sysroot/include" als impliziten
# Compiler-Suchpfad von emcc und liesse ihn bei target_include_directories()
# aus den generierten -I-Flags herausfallen -- und damit auch aus
# compile_commands.json. Als rohe Compile-Option bleibt er erhalten, sodass
# clangd/vim SDL2/SDL_image-Header findet.
#
# --target=wasm32-unknown-emscripten wird von emcc ebenfalls nur intern an
# clang durchgereicht. Ohne es faellt clangd auf das Host-Target (macOS/
# Mach-O) zurueck und meldet bei EM_JS/EM_ASM (__attribute__((section("em_js"))))
# faelschlich "mach-o section specifier requires a segment and section
# separated by a comma", weil dieses Section-Format nur fuer Wasm/ELF gilt.
#
# -std=gnu2x: emscripten/promise.h nutzt die C23-Attribut-Syntax [[nodiscard]].
# Das von emcc intern verwendete, sehr aktuelle clang akzeptiert das ohne
# explizites -std, aeltere clangd-Versionen (z.B. das in YouCompleteMe
# gebuendelte clangd 14) aber nicht -- dort kommt sonst "expected expression"
# in promise.h.
#
# -sUSE_SDL_IMAGE=2 auch hier: emcc legt die Header eines Ports erst in den
# Sysroot, wenn er ihn zum ersten Mal zieht -- sonst erst beim Linken, und
# rendering/iso_tile_images.c faende SDL2/SDL_image.h davor nicht.
target_compile_options(client PRIVATE
    "-I${EMSCRIPTEN_SYSROOT}/include"
    "--target=wasm32-unknown-emscripten"
    "-std=gnu2x"
    "-sUSE_SDL_IMAGE=2"
)

# Bilder und Schrift stecken nicht im Wasm, sondern liegen einzeln mit Hash im
# Namen unter web/assets/, dazu manifest.json (siehe publish_assets.cmake). So
# kann der Browser jede Datei fuer sich cachen, und eine Code-Aenderung laedt
# nicht alle Bilder neu. Das Frontend legt sie vor main() ins Dateisystem des
# Moduls (preRun, ueber das exportierte FS).
#
# Ein eigenes Ziel statt POST_BUILD an client: es laeuft bei jedem Bau, auch
# wenn sich nur ein Bild geaendert hat und client nicht neu gelinkt wird.
# assets/ und web/ liegen weiterhin neben den Unterprojekten, nicht in client/.
add_custom_target(client_assets ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DASSETS_DIR=${PROJECT_SOURCE_DIR}/assets"
        "-DOUTPUT_DIR=${PROJECT_SOURCE_DIR}/web/assets"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/publish_assets.cmake"
    COMMENT "Assets nach web/assets/ (mit Hash im Namen)"
    VERBATIM
)
add_dependencies(client client_assets)

target_link_options(client PRIVATE
    # Kein -sUSE_SDL_TTF=2: Text zeichnet rendering/bitmap_font.c aus einem
    # Atlas. SDL_ttf braechte FreeType und HarfBuzz mit, rund 900 KB im Wasm.
    "-sUSE_SDL=2"
    "-sUSE_SDL_IMAGE=2"
    "-sSDL2_IMAGE_FORMATS=[\"png\"]"
    # Ohne --embed-file sieht emcc keine eingebetteten Dateien mehr; das volle
    # Dateisystem soll trotzdem sicher mitkommen, das Frontend schreibt hinein.
    "-sFORCE_FILESYSTEM"
    "-sEXPORTED_FUNCTIONS=['_main','_RAD_OnMessageReceived','_zuc_on_connection_state','_RAD_ClientSetPlayerId','_malloc','_free']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','HEAPU8','FS']"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createZucchiniModule"
    "-sENVIRONMENT=web"
    "-sALLOW_MEMORY_GROWTH"
)

set_target_properties(client PROPERTIES
    OUTPUT_NAME "client"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/web"
)
