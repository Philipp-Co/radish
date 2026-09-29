# Erzeugt aus den .proto-Dateien hier im Verzeichnis per protoc/protoc-gen-c
# (siehe cmake/protobuf-c.cmake, das PROTOC_EXECUTABLE/PROTOC_GEN_C_EXECUTABLE
# und die protobuf-c-Laufzeit protobuf-c::protobuf-c bereitstellt) den
# C-Code fuer die Netzwerknachrichten und baut daraus eine Bibliothek
# radish_protobuf, die alles Weitere (client/) direkt gegen die generierten
# NetXyz-Typen linken kann.

set(PROTOBUF_PROTO_FILES
    "${CMAKE_CURRENT_SOURCE_DIR}/command.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/discover.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/event.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/game.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/message.proto"
    "${CMAKE_CURRENT_SOURCE_DIR}/tile.proto"
)

# Eigener Unterordner im Build-Verzeichnis dieses Ziels statt irgendwo direkt
# unter CMAKE_BINARY_DIR: haelt generierten Code von Hand-geschriebenem
# sauber getrennt und kollidiert nicht mit anderen add_custom_command-
# Ausgaben, falls spaeter weitere .proto-Gruppen dazukommen.
set(PROTOBUF_GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")

# protoc-gen-c benennt fuer "foo.proto" die Ausgabe "foo.pb-c.c"/"foo.pb-c.h"
# und legt sie relativ zu --proto_path unter --c_out ab -- da alle
# .proto-Dateien direkt unter proto_path liegen (kein Unterordner), landen
# die generierten Dateien direkt in PROTOBUF_GENERATED_DIR, ohne verschachtelte
# Pfade.
set(PROTOBUF_GENERATED_SOURCES
    "${PROTOBUF_GENERATED_DIR}/command.pb-c.c"
    "${PROTOBUF_GENERATED_DIR}/discover.pb-c.c"
    "${PROTOBUF_GENERATED_DIR}/event.pb-c.c"
    "${PROTOBUF_GENERATED_DIR}/game.pb-c.c"
    "${PROTOBUF_GENERATED_DIR}/message.pb-c.c"
    "${PROTOBUF_GENERATED_DIR}/tile.pb-c.c"
)
set(PROTOBUF_GENERATED_HEADERS
    "${PROTOBUF_GENERATED_DIR}/command.pb-c.h"
    "${PROTOBUF_GENERATED_DIR}/discover.pb-c.h"
    "${PROTOBUF_GENERATED_DIR}/event.pb-c.h"
    "${PROTOBUF_GENERATED_DIR}/game.pb-c.h"
    "${PROTOBUF_GENERATED_DIR}/message.pb-c.h"
    "${PROTOBUF_GENERATED_DIR}/tile.pb-c.h"
)

# Ein einziger protoc-Aufruf fuer alle Dateien, nicht einer je Datei:
# message.proto importiert event.proto/command.proto/discover.proto, event.proto importiert
# tile.proto/game.proto -- protoc loest das nur auf, wenn die importierten
# Dateien im selben Lauf mit angegeben sind (oder zumindest ueber
# --proto_path auffindbar), unabhaengig von der Reihenfolge in der Liste
# oben.
#
# OUTPUT nennt alle erzeugten Dateien einzeln: nur so weiss CMake, dass
# dieser eine Custom-Command-Aufruf sie alle liefert, und kann den Schritt
# beim naechsten Build ueberspringen, wenn sich keine .proto-Datei geaendert
# hat (DEPENDS).
add_custom_command(
    OUTPUT ${PROTOBUF_GENERATED_SOURCES} ${PROTOBUF_GENERATED_HEADERS}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${PROTOBUF_GENERATED_DIR}"
    COMMAND ${PROTOC_EXECUTABLE}
            "--plugin=protoc-gen-c=${PROTOC_GEN_C_EXECUTABLE}"
            "--proto_path=${CMAKE_CURRENT_SOURCE_DIR}"
            "--c_out=${PROTOBUF_GENERATED_DIR}"
            ${PROTOBUF_PROTO_FILES}
    DEPENDS ${PROTOBUF_PROTO_FILES}
    COMMENT "protoc: generiere C-Code aus radish/protobuf/*.proto"
    VERBATIM
)

add_library(radish_protobuf STATIC ${PROTOBUF_GENERATED_SOURCES})

# PUBLIC: die generierten Header (${PROTOBUF_GENERATED_DIR}/*.pb-c.h) binden
# sich gegenseitig ueber genau diesen Pfad ein (z.B. message.pb-c.h
# gegenueber event.pb-c.h/command.pb-c.h), und wer radish_protobuf benutzt,
# muss dieselben Header finden koennen, um z.B. NetUserRequest zu bauen.
target_include_directories(radish_protobuf PUBLIC "${PROTOBUF_GENERATED_DIR}")

# Ebenfalls PUBLIC: jede generierte .pb-c.h-Datei bindet
# "protobuf-c/protobuf-c.h" ein und nutzt dessen Typen (ProtobufCMessage
# usw.) im eigenen Interface -- anders als bei radish_game/jsmn (siehe
# game/CMakeLists.txt) ist protobuf-c hier also Teil der oeffentlichen
# Schnittstelle dieser Bibliothek, nicht nur ihrer Implementierung.
target_link_libraries(radish_protobuf PUBLIC protobuf-c::protobuf-c)

# Generierter Code, dessen Warnungen uns nichts sagen -- gleiches Vorgehen
# wie bei protobuf_c selbst (cmake/protobuf-c.cmake) und bei unity.cmake.
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(radish_protobuf PRIVATE -w)
endif()
