#ifndef __RAD_SERIALIZATION_H__
#define __RAD_SERIALIZATION_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <radish/game/game.h>

///
/// Was die Leser unter serialization/ gemeinsam haben: ihr Ergebnis und die Grenze,
/// bis zu der eine Datei gelesen wird. Den einzigen Leser, den es gibt, beschreibt
/// world_definition.h; den Token-Lauf, auf dem er steht, json_reader.h.
///

///
/// Obergrenze fuer eine Datei, die ganz in den Speicher gelesen wird. Eine
/// Weltdefinition braucht bei 8x8 Feldern zwei Raster aus je acht Zeilen, dazu
/// Schluessel und Einrueckung -- weit unter einem Kilobyte. 32 KB lassen Platz
/// fuer eine groessere Welt, ohne dass eine fremde Datei beliebig viel Speicher
/// verlangen kann.
///
#define RAD_JSON_FILE_MAX (32 * 1024)

typedef enum
{
    RAD_SERIALIZE_OK = 0,

    /// Kein gueltiges JSON.
    RAD_SERIALIZE_ERROR_SYNTAX,
    /// Gueltiges JSON, aber nicht die erwartete Struktur.
    RAD_SERIALIZE_ERROR_SCHEMA,

    RAD_SERIALIZE_ERROR_VERSION,
    RAD_SERIALIZE_ERROR_SIZE_MISMATCH,

    RAD_SERIALIZE_ERROR_TILE_TYPE,

    /// Die Datei widerspricht sich selbst -- eine Hoehe auf einem Feld ohne
    /// Gelaende (world_definition.h).
    RAD_SERIALIZE_ERROR_INCONSISTENT,

    /// Eine Weltdefinition soll eine Welt ersetzen, auf der schon Figuren stehen
    /// (world_definition.h).
    RAD_SERIALIZE_ERROR_WORLD_OCCUPIED,

    RAD_SERIALIZE_ERROR_OUT_OF_MEMORY
} RAD_SerializeResult_t;

const char* RAD_SerializeResultText(RAD_SerializeResult_t result);

#endif
