cmake_minimum_required(VERSION 3.16)
project(zucchini_client C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT EMSCRIPTEN)
    message(FATAL_ERROR "This project must be configured with emcmake (Emscripten toolchain).")
endif()

add_executable(client
    src/main.c
    src/rendering/render_object.c
    src/rendering/iso_object.c
    src/rendering/iso_map.c
)

# emcc fuegt den Header-Suchpfad fuer per USE_SDL=2/USE_SDL_TTF=2 gezogene Ports
# nur intern beim eigentlichen Aufruf hinzu -- er taucht nicht in den Flags auf,
# die CMake in compile_commands.json schreibt. Fuer clangd/vim muss er deshalb
# hier explizit ergaenzt werden.
execute_process(
    COMMAND em-config CACHE
    OUTPUT_VARIABLE EMSCRIPTEN_CACHE_DIR
    OUTPUT_STRIP_TRAILING_WHITESPACE
)

target_include_directories(client PRIVATE
    include
    src/include
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
    "-I${EMSCRIPTEN_CACHE_DIR}/sysroot/include"
    "--target=wasm32-unknown-emscripten"
    "-std=gnu2x"
)

set(ZUC_FONT_PATH "${CMAKE_SOURCE_DIR}/assets/LiberationSansBold.ttf")

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
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/web"
)

add_custom_command(TARGET client POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_BINARY_DIR}/compile_commands.json"
            "${CMAKE_SOURCE_DIR}/compile_commands.json"
    COMMENT "Copying compile_commands.json to radish/ for vim/clangd"
)
