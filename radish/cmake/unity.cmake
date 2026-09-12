# Unity liegt als eigenstaendiger Klon neben dem Projekt und wird nicht kopiert.
# Fehlt er, soll das sofort und deutlich scheitern statt spaeter als
# "unity.h: file not found" mitten im Build. Dasselbe Muster wie bei jsmn und
# zucchini; das Verzeichnis heisst dort oben "Unity", mit grossem U.
get_filename_component(UNITY_DEFAULT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../Unity" ABSOLUTE)
set(UNITY_DIR "${UNITY_DEFAULT_DIR}" CACHE PATH "Pfad zum Unity-Klon")

if(NOT EXISTS "${UNITY_DIR}/src/unity.c")
    message(FATAL_ERROR "Unity nicht gefunden unter ${UNITY_DIR} -- dort 'git clone https://github.com/ThrowTheSwitch/Unity' ausfuehren.")
endif()

# Eigenes Ziel statt add_subdirectory(${UNITY_DIR}): Unity bringt ein eigenes
# CMakeLists mit project(), Versionsableitung aus dem Header und Install-/Export-
# Regeln. Davon braucht dieser Build nichts -- eine Uebersetzungseinheit und ein
# Suchpfad sind alles, was Unity ist, und so bleibt auch die Kontrolle ueber die
# Warnungsschalter bei uns.
add_library(unity STATIC "${UNITY_DIR}/src/unity.c")
add_library(unity::framework ALIAS unity)

target_include_directories(unity PUBLIC "${UNITY_DIR}/src")

# Fremder Code, dessen Warnungen uns nichts sagen: -Wall -Wextra stehen im
# Wurzel-CMakeLists fuer alle Ziele, hier werden sie fuer dieses eine wieder
# abgeraeumt.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(unity PRIVATE -w)
endif()
