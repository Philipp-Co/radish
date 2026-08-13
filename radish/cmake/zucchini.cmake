# zucchini liegt wie jsmn als eigenstaendiger Klon neben dem Projekt und wird
# nicht kopiert. Fehlt er, soll das sofort und deutlich scheitern statt spaeter
# als "zucchini/api/api.h: file not found" mitten im Build.
#
# Anders als bei jsmn wird hier nicht bloss ein Suchpfad beigetragen: zwei
# Bibliotheken des Klons werden mitgebaut. Ihre Quellen bleiben dabei dort
# liegen, die Artefakte landen unter ${CMAKE_BINARY_DIR}/zucchini/.
get_filename_component(ZUCCHINI_DEFAULT_DIR "${CMAKE_CURRENT_LIST_DIR}/../../zucchini" ABSOLUTE)
set(ZUCCHINI_DIR "${ZUCCHINI_DEFAULT_DIR}" CACHE PATH "Pfad zum zucchini-Klon")

if(NOT EXISTS "${ZUCCHINI_DIR}/api/include/zucchini/api/api.h")
    message(FATAL_ERROR "zucchini nicht gefunden unter ${ZUCCHINI_DIR} -- den Klon dorthin legen oder -DZUCCHINI_DIR=<pfad> setzen.")
endif()

# Nur utils/ und api/, nicht das Dachprojekt zucchini/CMakeLists.txt: das ruft
# project(), verlangt ZUC_PLATFORM und baut Server, Admin-Client und die
# Testwerkzeuge mit -- vier Executables, die hier niemand braucht. Die beiden
# Bibliotheken fragen ZUC_PLATFORM nicht ab und tragen ihre Include-Pfade
# PUBLIC, laufen also fuer sich.
#
# Der zweite Parameter ist Pflicht: die Quellen liegen ausserhalb dieses Baums,
# CMake kann das Build-Verzeichnis daher nicht selbst ableiten.
add_subdirectory("${ZUCCHINI_DIR}/utils" "${CMAKE_BINARY_DIR}/zucchini/utils")
add_subdirectory("${ZUCCHINI_DIR}/api"   "${CMAKE_BINARY_DIR}/zucchini/api")

# In ihrem Heimatprojekt werden beide mit C23 uebersetzt -- das setzt
# zucchini/CMakeLists.txt global, und genau das wird hier uebersprungen. Der
# Standard wird deshalb pro Ziel nachgezogen: die Quellen brauchen ihn derzeit
# nicht, werden aber in einem Projekt gepflegt, in dem er gilt.
#
# radishs -Wall -Wextra gelten hier ebenfalls; Warnungen aus fremden Quellen
# sind moeglich, brechen den Build aber nicht ab.
set_target_properties(zucchini_utils zucchini_api PROPERTIES
    C_STANDARD 23
    C_STANDARD_REQUIRED ON
)
