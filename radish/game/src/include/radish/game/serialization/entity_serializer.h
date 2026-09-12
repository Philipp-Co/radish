#ifndef __RAD_ENTITY_SERIALIZER_H__
#define __RAD_ENTITY_SERIALIZER_H__

#include <stdbool.h>
#include <radish/game/model/entity/entity.h>
#include <radish/game/serialization/serialization.h>
#include <radish/game/serialization/json_writer.h>
#include <radish/game/serialization/json_reader.h>

///
/// Schema -- eine Entitaet ist ein JSON-Objekt:
///
///     { "id": 4, "type": "npc", "owner": "0x00000000000004d2", "x": 1, "y": 2 }
///
///   id       Slot-Index im Entitaeten-Pool der Welt, 0 bis RAD_MAX_ENTITIES-1.
///            Steht in der Datei, damit Verweise auf eine Entitaet das
///            Speichern ueberstehen: nach Loeschungen ist der Pool luecklenhaft,
///            und ein Neuvergeben beim Laden wuerde alle Ids verschieben.
///   type     "none", "player" oder "npc" -- die Namen aus
///            RAD_EntityTypeToString, nicht die Zahlwerte des enum.
///   owner    Uuid des Benutzers, dem die Figur gehoert, oder null fuer
///            herrenlos. Als String und nicht als Zahl, weil eine Uuid
///            vierundsechzig Bit breit ist -- die Begruendung steht bei
///            RAD_JsonWriteUInt64.
///   x, y     Tile, auf dem die Entitaet steht. Beide sind int16_t breit; was
///            nicht hineinpasst, ist RAD_SERIALIZE_ERROR_ENTITY_POSITION.
///
/// Unbekannte Schluessel werden beim Lesen uebergangen; ein fehlendes "owner"
/// heisst herrenlos. Ein Stand aus der Zeit vor dem Besitz laedt damit weiter,
/// und das Format bleibt bei Version 1.
///
/// Bildet eine einzelne Entitaet treu auf dieses Objekt ab und zurueck. Ob die
/// gelesene Id vergeben werden kann und das Ziel-Tile frei ist, entscheidet
/// der World-Serializer -- hier wird nur gelesen, was dasteht.
///
/// Die Breite der Koordinatenfelder ist die eine Ausnahme davon, und sie ist
/// keine: ein Wert, der nicht ins Feld passt, ist nicht gelesen, sondern
/// verstuemmelt. Ob er auf dem Raster liegt, prueft weiterhin der
/// World-Serializer -- mit demselben Fehlercode, weil es dieselbe Frage ist.
///
void RAD_SerializeEntity(RAD_JsonWriter_t *writer, const RAD_Entity_t *entity);
RAD_SerializeResult_t RAD_DeserializeEntity(RAD_JsonReader_t *reader, RAD_Entity_t *entity);

const char* RAD_EntityTypeToString(RAD_EntityType_t type);
RAD_EntityType_t RAD_EntityTypeFromString(const char *name, bool *ok);

#endif
