#ifndef __RAD_WORLD_H__
#define __RAD_WORLD_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/game_definitions.h>
#include <radish/game/tile.h>
#include <radish/game/entity.h>
#include <radish/game/events/event_manager.h>

typedef struct
{
    RAD_Tile_t tiles[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH];

    ///
    /// Pool mit Luecken: freie Slots tragen id == RAD_ENTITY_NONE. Der
    /// Array-Index ist die RAD_EntityId_t, weshalb Slots beim Entfernen nicht
    /// zusammengeschoben werden duerfen.
    ///
    RAD_Entity_t entities[RAD_MAX_ENTITIES];

    /// Anzahl belegter Slots, nicht der hoechste vergebene Index.
    int32_t number_of_entities;

    RAD_EventManager_t *event_manager;
} RAD_World_t;

RAD_World_t RAD_CreateWorld(RAD_EventManager_t *event_manager);
void RAD_InitWorld(RAD_World_t *world);

bool RAD_WorldInBounds(const RAD_World_t *world, int32_t x, int32_t y);
RAD_Tile_t* RAD_WorldTileAt(RAD_World_t *world, int32_t x, int32_t y);
RAD_Entity_t* RAD_WorldEntityById(RAD_World_t *world, RAD_EntityId_t id);
RAD_Entity_t* RAD_WorldEntityAt(RAD_World_t *world, int32_t x, int32_t y);

///
/// Die einzigen drei Funktionen, die eine Entitaetsposition schreiben duerfen.
/// Sie halten RAD_Entity_t.x/y und RAD_Tile_t.entity synchron und sichern damit
/// die Invariante "hoechstens eine Entitaet pro Tile".
///
RAD_EntityId_t RAD_WorldSpawnEntity(RAD_World_t *world, RAD_EntityType_t type, int32_t x, int32_t y);
bool RAD_WorldMoveEntity(RAD_World_t *world, RAD_EntityId_t id, int32_t x, int32_t y);
void RAD_WorldRemoveEntity(RAD_World_t *world, RAD_EntityId_t id);

///
/// Wie RAD_WorldSpawnEntity, aber mit vorgegebener Id statt dem naechsten freien
/// Slot. Wird beim Laden gebraucht, damit gespeicherte Ids erhalten bleiben --
/// nach Loeschungen ist der Pool luecklenhaft und ein Neuvergeben wuerde alle
/// Ids verschieben. Liefert RAD_ENTITY_NONE, wenn die Id ausserhalb des Pools
/// liegt, ihr Slot schon belegt ist oder das Ziel-Tile nicht frei ist.
///
RAD_EntityId_t RAD_WorldSpawnEntityWithId(RAD_World_t *world, RAD_EntityId_t id, RAD_EntityType_t type, int32_t x, int32_t y);

///
/// Besitz einer Entitaet lesen und setzen (RAD_Entity_t.owner).
///
/// Getrennt vom Setzen, statt als Parameter beim Setzen einer Figur: eine Figur
/// entsteht in der Welt, ein Benutzer steht aber nicht in ihr -- wer wem etwas
/// zuordnen darf, entscheidet das Spiel (RAD_GameBindEntity). Die Welt fuehrt das
/// Feld nur.
///
/// Ein Besitzerwechsel haelt keine zweite Angabe synchron und ist deshalb, anders
/// als eine Positionsaenderung, an keine der drei Funktionen oben gebunden.
/// Gesetzt wird ohne Pruefung, ob der Benutzer mitspielt oder die Figur schon
/// jemandem gehoert -- das sind Fragen des Spiels.
///
/// RAD_WorldEntityOwner liefert RAD_USER_NONE fuer eine Entitaet, die es nicht
/// gibt: sie gehoert niemandem, so wie eine herrenlose. RAD_WorldSetEntityOwner
/// liefert false, wenn es sie nicht gibt -- hier ist der Unterschied wichtig,
/// weil sonst ein Zuordnen ins Leere unbemerkt bliebe.
///
RAD_UserId_t RAD_WorldEntityOwner(const RAD_World_t *world, RAD_EntityId_t id);
bool RAD_WorldSetEntityOwner(RAD_World_t *world, RAD_EntityId_t id, RAD_UserId_t owner);

///
/// Prueft die Doppelbuchfuehrung zwischen Tiles und Entitaeten vollstaendig
/// gegeneinander, dazu die eine Zusage ueber den Besitz: ein freier Slot traegt
/// keinen Besitzer. Beim regulaeren Spielverlauf immer true -- interessant nach
/// dem Laden und als Zusicherung im Test.
///
bool RAD_WorldIsConsistent(const RAD_World_t *world);

#endif
