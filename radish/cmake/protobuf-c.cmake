# protobuf-c liegt wie jsmn/Unity/zucchini als eigenstaendiger Klon neben dem
# Projekt und wird nicht kopiert. Fehlt er, soll das sofort und deutlich
# scheitern statt spaeter als "protobuf-c/protobuf-c.h: file not found" mitten
# im Build.
get_filename_component(PROTOBUF_C_DEFAULT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../protobuf-c" ABSOLUTE)
set(PROTOBUF_C_DIR "${PROTOBUF_C_DEFAULT_DIR}" CACHE PATH "Pfad zum protobuf-c-Klon")

if(NOT EXISTS "${PROTOBUF_C_DIR}/protobuf-c/protobuf-c.h")
    message(FATAL_ERROR "protobuf-c nicht gefunden unter ${PROTOBUF_C_DIR} -- dort 'git clone https://github.com/protobuf-c/protobuf-c' ausfuehren.")
endif()

# Nur die Laufzeit (protobuf-c/protobuf-c.c und .h) wird aus dem Klon gebaut,
# nicht dessen eigenes Autotools-/CMake-Projekt (Doxygen, pkg-config-Dateien,
# Tests, ...) -- dieselbe Haltung wie bei unity.cmake: eine
# Uebersetzungseinheit und ein Suchpfad sind alles, was hier gebraucht wird.
#
# Der Codegenerator protoc-gen-c (protoc-gen-c/ im Klon, C++, haengt an der
# vollen Protobuf-C++-Bibliothek) wird bewusst NICHT aus diesem Klon gebaut --
# das waere fuer dieses Projekt unverhaeltnismaessig schwer. Er wird
# stattdessen als bereits installiertes Host-Werkzeug vorausgesetzt (siehe
# PROTOC_GEN_C_EXECUTABLE unten), z.B. per "brew install protobuf-c" oder
# ueber die Paketverwaltung der jeweiligen Distribution.
add_library(protobuf_c STATIC "${PROTOBUF_C_DIR}/protobuf-c/protobuf-c.c")
add_library(protobuf-c::protobuf-c ALIAS protobuf_c)

# PUBLIC auf den Klon-Wurzelordner, nicht auf protobuf-c/protobuf-c/ selbst:
# generierter wie eigener Code bindet ueber "#include <protobuf-c/protobuf-c.h>"
# ein (protobuf-c generiert Code, der genau diesen Pfad erwartet) -- der
# Anteil "protobuf-c/" gehoert also mit zum Include-Pfad.
target_include_directories(protobuf_c PUBLIC "${PROTOBUF_C_DIR}")

# Fremder Code, dessen Warnungen uns nichts sagen: -Wall -Wextra stehen im
# Wurzel-CMakeLists fuer alle Ziele, hier werden sie fuer dieses eine wieder
# abgeraeumt (gleiches Vorgehen wie bei unity.cmake).
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(protobuf_c PRIVATE -w)
endif()

# Host-Werkzeuge fuer den Codegen-Schritt (siehe radish/protobuf/, noch nicht
# Teil dieser Datei/dieses Schritts): protoc selbst plus dessen C-Plugin
# protoc-gen-c. Beide werden schon hier gesucht und mit einer klaren
# Fehlermeldung abgebrochen, falls sie fehlen -- lieber jetzt beim
# Konfigurieren scheitern als spaeter mitten im Build mit "protoc-gen-c:
# Plugin failed with status code 1" oder aehnlich Kryptischem.
#
# find_program sucht trotz Cross-Compile unter Emscripten im Host-PATH:
# CMAKE_FIND_ROOT_PATH_MODE_PROGRAM steht bei einem cross-kompilierenden
# Toolchain-File (wie Emscriptens) standardmaessig auf NEVER, sucht also
# ausserhalb des Sysroots -- genau richtig fuer ein Werkzeug, das auf dem Host
# laeuft und nicht Teil des Wasm-Builds ist.
find_program(PROTOC_EXECUTABLE protoc)
if(NOT PROTOC_EXECUTABLE)
    message(FATAL_ERROR "protoc nicht gefunden -- z.B. per 'brew install protobuf' installieren.")
endif()

find_program(PROTOC_GEN_C_EXECUTABLE protoc-gen-c)
if(NOT PROTOC_GEN_C_EXECUTABLE)
    message(FATAL_ERROR "protoc-gen-c nicht gefunden -- z.B. per 'brew install protobuf-c' installieren.")
endif()
