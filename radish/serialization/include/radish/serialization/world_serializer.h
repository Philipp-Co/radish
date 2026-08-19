#ifndef __RAD_WORLD_SERIALIZER_H__
#define __RAD_WORLD_SERIALIZER_H__

#include <radish/game/model/world/world.h>
#include <radish/serialization/serialization.h>
#include <radish/serialization/json_writer.h>
#include <radish/serialization/json_reader.h>

///
/// Schema -- die Welt ist ein JSON-Objekt:
///
///     {
///       "width": 8,
///       "height": 8,
///       "tiles": [ [ { "x": 0, "y": 0, ... }, ... ], ... ],
///       "entities": [ { "id": 0, ... }, ... ]
///     }
///
///   width,   Weltgroesse. Muss RAD_WORLD_WIDTH bzw. RAD_WORLD_HEIGHT
///   height   entsprechen, sonst RAD_SERIALIZE_ERROR_SIZE_MISMATCH.
///   tiles    "height" Zeilen zu je "width" Tile-Objekten, Zeile 0 zuerst --
///            also in der Reihenfolge von world->tiles[y][x]. Siehe
///            tile_serializer.h.
///   entities Nur lebende Entitaeten, aufsteigend nach Id. Freie Pool-Slots
///            kommen nicht vor: sie sind ein Detail der Ablage, kein Objekt des
///            Spiels. Siehe entity_serializer.h.
///
/// Eine Anzahl der Entitaeten steht bewusst nicht in der Datei -- sie ist die
/// Laenge von "entities". Ein zusaetzlicher Zaehler koennte nur sich selbst
/// widersprechen und waere beim Editieren von Hand eine Falle; alles, was echte
/// Korruption waere, faellt ohnehin bei der Gegenprobe unten auf.
///
/// Die Reihenfolge der Schluessel spielt beim Lesen keine Rolle, unbekannte
/// werden uebergangen.
///
/// Wer auf welchem Tile steht, steht doppelt in der Datei: einmal als "entity"
/// am Tile, einmal ueber "x"/"y" der Entitaet. Das ist Absicht -- jedes Objekt
/// wird vollstaendig abgebildet -- verlangt beim Lesen aber eine Gegenprobe,
/// siehe RAD_DeserializeWorld.
///
void RAD_SerializeWorld(RAD_JsonWriter_t *writer, const RAD_World_t *world);

///
/// Liest die Welt und baut die Entitaeten ueber RAD_WorldSpawnEntityWithId
/// wieder auf, statt den Pool direkt zu beschreiben. Die Entitaets-Zuordnung
/// aus der Datei wird anschliessend gegen die so entstandene geprueft: beide
/// Seiten muessen uebereinstimmen, sonst RAD_SERIALIZE_ERROR_INCONSISTENT.
///
/// Die Reihenfolge der Felder im JSON spielt keine Rolle.
///
RAD_SerializeResult_t RAD_DeserializeWorld(RAD_JsonReader_t *reader, RAD_World_t *world);

#endif
