#ifndef __RAD_SERIALIZATION_H__
#define __RAD_SERIALIZATION_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <radish/game/game.h>

///
/// Speicherformat -- jedes Objekt des Spiels wird auf ein JSON-Objekt abgebildet.
/// Zu jedem Typ gehoert ein eigener Serializer, der genau seinen Ausschnitt
/// beschreibt; das Wurzeldokument sieht so aus:
///
///     {
///       "format": "radish-save",
///       "version": 1,
///       "game": {
///         "world": {
///           "width": 8,
///           "height": 8,
///           "tiles": [
///             [
///               { "x": 0, "y": 0, "type": "ground", "entity": null },
///               { "x": 1, "y": 0, "type": "water",  "entity": null }
///               // ... "width" Tiles je Zeile
///             ]
///             // ... "height" Zeilen
///           ],
///           "entities": [
///             { "id": 0, "type": "player", "x": 3, "y": 4 }
///           ]
///         }
///       }
///     }
///
///   format   Muss RAD_SAVE_FORMAT_NAME sein, sonst RAD_SERIALIZE_ERROR_FORMAT.
///   version  Muss RAD_SAVE_FORMAT_VERSION sein, sonst RAD_SERIALIZE_ERROR_VERSION.
///   game     Der Spielzustand, siehe game_serializer.h.
///
/// Beides wird geprueft, bevor "game" ueberhaupt gelesen wird -- eine fremde
/// Datei meldet so "fremdes Format" statt eines Strukturfehlers tief in der Welt.
///
/// Die einzelnen Ebenen sind dokumentiert in game_serializer.h,
/// world_serializer.h, tile_serializer.h und entity_serializer.h. Durchgaengig
/// gilt: Aufzaehlungen stehen als Name statt als Zahl, damit ein spaeter
/// eingefuegter enum-Wert gespeicherte Staende nicht umdeutet, und unbekannte
/// Schluessel werden beim Lesen uebergangen, damit ein spaeter hinzugefuegtes
/// Feld aeltere Staende nicht entwertet.
///
#define RAD_SAVE_FORMAT_NAME "radish-save"
#define RAD_SAVE_FORMAT_VERSION 1

///
/// Obergrenze fuer den Schreibpuffer. Ein Tile-Objekt braucht rund 46 Zeichen,
/// ein Entitaets-Objekt rund 37; bei 8x8 Tiles und vollem Pool sind das knapp
/// 6 KB kompakt. Mit Einrueckung wird es etwa dreimal so viel, 32 KB decken
/// beide Faelle mit Reserve ab.
///
#define RAD_SAVE_JSON_MAX (32 * 1024)

typedef enum
{
    RAD_SERIALIZE_OK = 0,

    /// Der Schreibpuffer war zu klein.
    RAD_SERIALIZE_ERROR_BUFFER_TOO_SMALL,
    /// Kein gueltiges JSON.
    RAD_SERIALIZE_ERROR_SYNTAX,
    /// Gueltiges JSON, aber nicht die erwartete Struktur.
    RAD_SERIALIZE_ERROR_SCHEMA,

    RAD_SERIALIZE_ERROR_FORMAT,
    RAD_SERIALIZE_ERROR_VERSION,
    RAD_SERIALIZE_ERROR_SIZE_MISMATCH,

    RAD_SERIALIZE_ERROR_TILE_TYPE,
    RAD_SERIALIZE_ERROR_ENTITY_TYPE,
    RAD_SERIALIZE_ERROR_ENTITY_ID,
    RAD_SERIALIZE_ERROR_ENTITY_POSITION,
    RAD_SERIALIZE_ERROR_TILE_OCCUPIED,

    /// Die Datei widerspricht sich selbst -- etwa wenn ein Tile auf eine andere
    /// Entitaet zeigt als die, die dort laut ihrer eigenen Position steht.
    RAD_SERIALIZE_ERROR_INCONSISTENT,

    RAD_SERIALIZE_ERROR_OUT_OF_MEMORY
} RAD_SerializeResult_t;

const char* RAD_SerializeResultText(RAD_SerializeResult_t result);

///
/// Schreibt das Spiel als JSON nach "buffer". "written" nimmt die Laenge ohne
/// die abschliessende Null auf und darf NULL sein.
///
RAD_SerializeResult_t RAD_SerializeGameToJson(const RAD_Game_t *game, char *buffer, size_t capacity, bool pretty, size_t *written);

///
/// Liest das Spiel aus JSON. Das Ziel wird erst ueberschrieben, wenn alles
/// gelesen und geprueft ist -- ein fehlgeschlagener Ladevorgang laesst ein
/// laufendes Spiel unangetastet.
///
RAD_SerializeResult_t RAD_DeserializeGameFromJson(RAD_Game_t *game, const char *json, size_t length);

#endif
