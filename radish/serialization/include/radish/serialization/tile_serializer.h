#ifndef __RAD_TILE_SERIALIZER_H__
#define __RAD_TILE_SERIALIZER_H__

#include <stdbool.h>
#include <radish/game/model/tile/tile.h>
#include <radish/serialization/serialization.h>
#include <radish/serialization/json_writer.h>
#include <radish/serialization/json_reader.h>

///
/// Schema -- ein Tile ist ein JSON-Objekt:
///
///     { "x": 3, "y": 5, "type": "water", "entity": 7 }
///
///   x, y     Position im Raster. Wird beim Lesen gegen die Stelle geprueft, an
///            der das Tile im "tiles"-Raster steht; weichen sie ab, ist die
///            Datei widerspruechlich (RAD_SERIALIZE_ERROR_INCONSISTENT).
///   type     "void", "ground" oder "water" -- die Namen aus
///            RAD_TileTypeToString, nicht die Zahlwerte des enum. Ein spaeter
///            in der Mitte eingefuegter Tile-Typ deutet gespeicherte Staende so
///            nicht stillschweigend um.
///   entity   Id der Entitaet auf diesem Tile, oder null wenn es frei ist.
///            RAD_ENTITY_NONE (-1) erscheint nie als Zahl in der Datei.
///
/// Unbekannte Schluessel werden beim Lesen uebergangen, damit ein spaeter
/// hinzugefuegtes Feld aeltere Staende nicht entwertet.
///
/// Bildet ein einzelnes Tile treu auf dieses Objekt ab und zurueck. Prueft
/// dabei nichts ueber das Tile hinaus -- ob die gelesene Entitaets-Id zur
/// Entitaetsliste passt, entscheidet der World-Serializer.
///
void RAD_SerializeTile(RAD_JsonWriter_t *writer, const RAD_Tile_t *tile);
RAD_SerializeResult_t RAD_DeserializeTile(RAD_JsonReader_t *reader, RAD_Tile_t *tile);

///
/// Namen statt Zahlen, damit ein spaeter in der Mitte eingefuegter Tile-Typ
/// gespeicherte Staende nicht stillschweigend umdeutet.
///
const char* RAD_TileTypeToString(RAD_TileType_t type);
RAD_TileType_t RAD_TileTypeFromString(const char *name, bool *ok);

#endif
