# jsmn liegt als eigenstaendiger Klon neben dem Projekt und wird nicht kopiert.
# Fehlt er, soll das sofort und deutlich scheitern statt spaeter als
# "jsmn.h: file not found" mitten im Build.
get_filename_component(JSMN_DEFAULT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../jsmn" ABSOLUTE)
set(JSMN_INCLUDE_DIR "${JSMN_DEFAULT_DIR}" CACHE PATH "Pfad zum jsmn-Klon")

if(NOT EXISTS "${JSMN_INCLUDE_DIR}/jsmn.h")
    message(FATAL_ERROR "jsmn nicht gefunden unter ${JSMN_INCLUDE_DIR} -- dort 'git clone https://github.com/zserge/jsmn' ausfuehren.")
endif()

# Header-only: jsmn traegt nur seinen Suchpfad bei. Die eine Uebersetzungs-
# einheit, die die Implementierung erzeugt, ist jsmn_impl.c in radish_serialization.
add_library(jsmn INTERFACE)
add_library(jsmn::jsmn ALIAS jsmn)

target_include_directories(jsmn INTERFACE "${JSMN_INCLUDE_DIR}")
