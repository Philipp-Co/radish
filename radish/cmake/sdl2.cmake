# SDL2 und SDL_image fuer den Wasm-Client, aus eigenen Klonen statt der
# Emscripten-Ports (-sUSE_SDL=2 / -sUSE_SDL_IMAGE=2). Die Ports uebersetzen eine
# feste Quellliste samt Audio, Joystick, Software-Renderer und libpng/zlib; hier
# bleibt nur, was der Client braucht. client.wasm schrumpft damit von rund
# 1 MB auf rund 390 KB.
#
# Beide liegen wie jsmn/protobuf-c als eigenstaendige Klone neben dem Projekt,
# auf festen Release-Tags. Fehlt einer, soll das sofort und deutlich scheitern
# statt spaeter als "SDL2/SDL.h: file not found" mitten im Build.
#
# Nur unter Emscripten einzubinden (radish/CMakeLists.txt): der Host-Build
# braucht kein SDL.
get_filename_component(SDL2_DEFAULT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../SDL" ABSOLUTE)
get_filename_component(SDL2_IMAGE_DEFAULT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../SDL_image" ABSOLUTE)
set(SDL2_DIR_SOURCE "${SDL2_DEFAULT_DIR}" CACHE PATH "Pfad zum SDL-Klon (Tag release-2.32.10)")
set(SDL2_IMAGE_DIR_SOURCE "${SDL2_IMAGE_DEFAULT_DIR}" CACHE PATH "Pfad zum SDL_image-Klon (Tag release-2.6.0)")

if(NOT EXISTS "${SDL2_DIR_SOURCE}/include/SDL.h")
    message(FATAL_ERROR "SDL nicht gefunden unter ${SDL2_DIR_SOURCE} -- dort 'git clone --depth 1 --branch release-2.32.10 https://github.com/libsdl-org/SDL.git SDL' ausfuehren.")
endif()
if(NOT EXISTS "${SDL2_IMAGE_DIR_SOURCE}/SDL_image.h")
    message(FATAL_ERROR "SDL_image nicht gefunden unter ${SDL2_IMAGE_DIR_SOURCE} -- dort 'git clone --depth 1 --branch release-2.6.0 https://github.com/libsdl-org/SDL_image.git SDL_image' ausfuehren.")
endif()

# SDL2 laeuft ohne LTO: sein altes cmake_minimum_required laesst CMP0069 auf
# OLD, und CMake ignoriert dann CMAKE_INTERPROCEDURAL_OPTIMIZATION
# (radish/CMakeLists.txt). So soll es bleiben -- mit
# CMAKE_POLICY_DEFAULT_CMP0069 NEW wuchs client.wasm von 388 auf 411 KB.

# --- SDL2 -------------------------------------------------------------------
#
# Nur statisch, ohne Testbibliothek, SDL2main und Installationsziele.
set(SDL_STATIC ON CACHE BOOL "" FORCE)
set(SDL_SHARED OFF CACHE BOOL "" FORCE)
set(SDL_TEST OFF CACHE BOOL "" FORCE)
set(SDL2_DISABLE_SDL2MAIN ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_INSTALL ON CACHE BOOL "" FORCE)
set(SDL2_DISABLE_UNINSTALL ON CACHE BOOL "" FORCE)

# Der Client braucht Video, Render, Events, Timer und Dateien (view/view.c,
# SDL_INIT_VIDEO). Alles andere bleibt draussen; Threads gibt es im Wasm ohne
# -pthread ohnehin nicht.
foreach(_subsystem AUDIO JOYSTICK HAPTIC HIDAPI SENSOR POWER LOCALE MISC LOADSO THREADS DISKAUDIO DUMMYAUDIO)
    set(SDL_${_subsystem} OFF CACHE BOOL "" FORCE)
endforeach()

add_subdirectory("${SDL2_DIR_SOURCE}" "${CMAKE_BINARY_DIR}/sdl2" EXCLUDE_FROM_ALL)

# SDL_LEAN_AND_MEAN (src/SDL_internal.h) nimmt heraus, was sich nicht per
# CMake-Option abschalten laesst: den Software-Renderer, YUV-Formate und die
# optimierten Blitter. Gezeichnet wird ohnehin mit dem GLES2-Renderer (WebGL,
# SDL_RENDERER_ACCELERATED in view/view.c).
#
# Die Folge: SDL wandelt Pixelformate nur noch eingeschraenkt um. Das geht gut,
# solange jedes Bild unter radish/assets/ ein RGBA-PNG ist -- stb_image liefert
# es als RGBA32, und GLES2 nimmt das ohne Umwandlung als Textur. Ein PNG ohne
# Alphakanal oder mit Palette kann dagegen in IMG_LoadTexture mit "Blit
# combination not supported" scheitern; das steht dann beim Start in der
# Konsole (rendering/iso_*_images.c, rendering/bitmap_font.c).
target_compile_definitions(SDL2-static PRIVATE SDL_LEAN_AND_MEAN=1)

# --- SDL_image --------------------------------------------------------------
#
# Nur PNG, geladen ueber das mitgelieferte stb_image (IMG_stb.c) statt ueber
# libpng und zlib; kein Speichern. Die Abhaengigkeit auf SDL2::SDL2-static
# findet SDL_image ueber das Ziel von oben.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_INSTALL OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_VENDORED OFF CACHE BOOL "" FORCE)
set(SDL2IMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG ON CACHE BOOL "" FORCE)
set(SDL2IMAGE_PNG_SAVE OFF CACHE BOOL "" FORCE)
foreach(_format AVIF BMP GIF JPG JXL LBM PCX PNM QOI SVG TGA TIF WEBP XCF XPM XV)
    set(SDL2IMAGE_${_format} OFF CACHE BOOL "" FORCE)
endforeach()

add_subdirectory("${SDL2_IMAGE_DIR_SOURCE}" "${CMAKE_BINARY_DIR}/sdl2_image" EXCLUDE_FROM_ALL)

# Der Client schreibt <SDL2/SDL_image.h>, SDL_image legt seinen Header aber
# flach in den Klon. Ohne diesen Ordner fiele das Include unbemerkt auf eine
# Port-Kopie im Emscripten-Sysroot zurueck, falls dort noch eine liegt.
set(SDL2_IMAGE_INCLUDE_DIR "${CMAKE_BINARY_DIR}/sdl2_image_include")
file(MAKE_DIRECTORY "${SDL2_IMAGE_INCLUDE_DIR}/SDL2")
configure_file("${SDL2_IMAGE_DIR_SOURCE}/SDL_image.h" "${SDL2_IMAGE_INCLUDE_DIR}/SDL2/SDL_image.h" COPYONLY)
