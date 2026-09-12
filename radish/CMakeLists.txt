cmake_minimum_required(VERSION 3.16)

project(radish LANGUAGES C)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Die Tests sind Teil des Host-Builds, aber nicht jeder Host-Build will sie: das
# Backend-Image uebersetzt denselben Quellbaum, braucht davon aber nur den Server
# und traegt den Unity-Klon nicht im Kontext. Ohne diesen Schalter muesste er
# mitkopiert werden -- eine Abhaengigkeit, die im Image nie etwas tut, und der
# Bau des Servers haenge am Zustand der Testsuite.
#
# Dieselbe Form wie ZUC_BUILD_REMOTE_CLIENT_TEST in zucchini, das der Backend-Bau
# aus demselben Grund abschaltet.
option(RAD_BUILD_TESTS "Die Unity-Testsuite mitbauen (nur ohne Emscripten)" ON)

if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    add_compile_options(-Wall -Wextra)
endif()

# Vor add_subdirectory(game): radish_game bindet die Serialisierung ein und
# braucht jsmn::jsmn beim Konfigurieren seines Ziels.
include(cmake/jsmn.cmake)

add_subdirectory(game)

# Zwei ausfuehrbare Ziele, die sich gegenseitig ausschliessen: der Client haengt
# an Emscripten und laeuft als Wasm im Browser, der Server haengt an der
# Zucchini-Api (mkfifo, poll, POSIX-Shared-Memory) und laeuft auf dem Host.
# game/ zieht keines von beidem und wird immer gebaut.
#
# cmake/zucchini.cmake steht bewusst hier im else()-Zweig und nicht oben neben
# jsmn: unter emcc liesse sich zucchini_api nicht uebersetzen, und ein
# Wasm-Build soll nicht an einem fehlenden zucchini-Klon scheitern.
if(EMSCRIPTEN)
    add_subdirectory(client)
else()
    include(cmake/zucchini.cmake)
    add_subdirectory(server)

    # Tests laufen auf der Maschine, nicht im Browser: sie sind Programme, die
    # der Host startet und deren Rueckgabewert ctest liest. Unter emcc gaebe es
    # nichts auszufuehren, deshalb steht beides hier und nicht oben.
    #
    # enable_testing() muss im Wurzel-CMakeLists stehen, sonst findet ctest die
    # Tests aus den Unterverzeichnissen nicht.
    #
    # cmake/unity.cmake steht mit im if(): es bricht ab, wenn der Klon fehlt, und
    # genau das soll ein Build ohne Tests nicht tun.
    if(RAD_BUILD_TESTS)
        include(cmake/unity.cmake)
        enable_testing()

        add_subdirectory(game/test)
    endif()
endif()

# compile_commands.json haengt nicht mehr am Client-Target, sondern am Build als
# Ganzem, damit auch die Eintraege der Bibliotheken sicher mitkommen.
#
# Nur unter Emscripten kopieren: die Datei in radish/ ist die, aus der clangd im
# Editor die SDL2- und Emscripten-Header aufloest. Ein Host-Build wuerde sie
# durch Eintraege ohne diese Suchpfade ersetzen und clangd damit blind machen.
if(EMSCRIPTEN)
    add_custom_target(copy_compile_commands ALL
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_BINARY_DIR}/compile_commands.json"
                "${CMAKE_CURRENT_SOURCE_DIR}/compile_commands.json"
        COMMENT "Copying compile_commands.json to radish/ for vim/clangd"
    )
endif()
