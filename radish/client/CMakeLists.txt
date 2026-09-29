# emcc fuegt den Header-Suchpfad fuer per USE_SDL=2/USE_SDL_TTF=2 gezogene Ports
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
    src/rendering/game_events/move.c
    
    src/io/user_input.c
    src/io/net_codec.c
    src/io/camera_control.c

    src/events/event_manager.c

    src/model/world.c
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
# clangd/vim SDL2/SDL_ttf-Header findet.
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
target_compile_options(client PRIVATE
    "-I${EMSCRIPTEN_SYSROOT}/include"
    "--target=wasm32-unknown-emscripten"
    "-std=gnu2x"
)

# assets/ und web/ liegen weiterhin neben den Unterprojekten, nicht in client/.
set(ZUC_FONT_PATH "${PROJECT_SOURCE_DIR}/assets/LiberationSansBold.ttf")

target_link_options(client PRIVATE
    "-sUSE_SDL=2"
    "-sUSE_SDL_TTF=2"
    "--embed-file" "${ZUC_FONT_PATH}@/assets/LiberationSansBold.ttf"
    "-sEXPORTED_FUNCTIONS=['_main','_zuc_on_response','_zuc_on_connection_state','_malloc','_free']"
    "-sEXPORTED_RUNTIME_METHODS=['ccall','HEAPU8']"
    "-sMODULARIZE=1"
    "-sEXPORT_NAME=createZucchiniModule"
    "-sENVIRONMENT=web"
    "-sALLOW_MEMORY_GROWTH"
)

set_target_properties(client PROPERTIES
    OUTPUT_NAME "client"
    RUNTIME_OUTPUT_DIRECTORY "${PROJECT_SOURCE_DIR}/web"
)
