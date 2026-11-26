# Legt die Assets des Clients einzeln neben client.js/client.wasm ab, statt sie
# per --embed-file ins Wasm zu packen (siehe CMakeLists.txt, Ziel
# client_assets). Aufruf im Skriptmodus:
#
#   cmake -DASSETS_DIR=<radish/assets> -DOUTPUT_DIR=<radish/web/assets>
#         -P publish_assets.cmake
#
# Jede Datei bekommt die ersten 12 Hexziffern ihres SHA-256 in den Namen
# (fonts/michroma/michroma_32.png -> fonts/michroma/michroma_32.3f9a1c0b7e2d.png). Ein
# geaendertes Bild hat damit einen neuen Namen, und das Backend darf jede
# Datei dauerhaft cachen lassen (radish/backend/web/views.py).
#
# manifest.json bildet den Pfad, unter dem der Client die Datei oeffnet, auf
# ihren Namen hier ab. Das Frontend laedt danach jede Datei und legt sie vor
# main() unter genau diesem Pfad ins Emscripten-Dateisystem
# (radish/frontend/src/app/shared/game-canvas/game-canvas.component.ts) -- der
# C-Code liest weiterhin "/assets/..." wie zuvor.

if(NOT ASSETS_DIR OR NOT OUTPUT_DIR)
    message(FATAL_ERROR "ASSETS_DIR und OUTPUT_DIR muessen gesetzt sein.")
endif()

# Frisch statt ergaenzt: sonst blieben Dateien mit altem Hash liegen.
file(REMOVE_RECURSE "${OUTPUT_DIR}")

# worlds/ und die Lizenzdatei liest der Client nicht. Einheiten-, Tile- und
# Effekt-Bilder gibt es im Build nicht: sie liegen samt sprite.json, tile.json
# bzw. effect.json in der Datenbank, und das Backend ergaenzt das Manifest um
# sie (radish/backend/web/interfaces/sprite_views.py) -- der Client findet sie
# unter /assets/units/<schluessel>/, /assets/tiles/<typ>/ und
# /assets/effects/<schluessel>/ im Dateisystem. Aus dem Build kommt nur die
# Schrift: dieselbe wie im Frontend (Michroma, SIL OFL, siehe
# assets/fonts/michroma/OFL.txt) -- als Bitmap-Atlas im BMFont-Textformat,
# erzeugt aus Michroma-Regular.ttf (rendering/bitmap_font.c). Die TTF selbst
# liest der Client nicht mehr.
set(asset_files
    "fonts/michroma/michroma_32.fnt"
    "fonts/michroma/michroma_32.png"
)
list(SORT asset_files)

set(manifest_entries "")
foreach(rel IN LISTS asset_files)
    file(SHA256 "${ASSETS_DIR}/${rel}" hash)
    string(SUBSTRING "${hash}" 0 12 short_hash)

    get_filename_component(dir "${rel}" DIRECTORY)
    get_filename_component(name "${rel}" NAME_WLE)
    get_filename_component(ext "${rel}" LAST_EXT)
    if(dir)
        set(hashed "${dir}/${name}.${short_hash}${ext}")
    else()
        set(hashed "${name}.${short_hash}${ext}")
    endif()

    configure_file("${ASSETS_DIR}/${rel}" "${OUTPUT_DIR}/${hashed}" COPYONLY)
    list(APPEND manifest_entries "  \"/assets/${rel}\": \"${hashed}\"")
endforeach()

list(JOIN manifest_entries ",\n" manifest_body)
file(WRITE "${OUTPUT_DIR}/manifest.json" "{\n${manifest_body}\n}\n")
