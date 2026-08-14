#ifndef __RAD_GAME_ENTITY_H__
#define __RAD_GAME_ENTITY_H__

#include <stdint.h>

///
/// Handle auf eine Entitaet: der Slot-Index im Entity-Pool der Welt. Der Index
/// bleibt ueber die Lebensdauer der Entitaet stabil, anders als ein Zeiger kann
/// er aber nicht baumeln und laesst sich unveraendert uebertragen.
///
typedef int32_t RAD_EntityId_t;

#define RAD_ENTITY_NONE ((RAD_EntityId_t)-1)

typedef enum
{
    RAD_ENTITY_TYPE_NONE = 0,
    RAD_ENTITY_TYPE_PLAYER,
    RAD_ENTITY_TYPE_NPC
} RAD_EntityType_t;

typedef struct
{
    ///
    /// Eigene Id, zugleich der Slot-Index im Pool. RAD_ENTITY_NONE markiert
    /// einen freien Slot.
    ///
    RAD_EntityId_t id;
    RAD_EntityType_t type;

    ///
    /// Tile, auf dem die Entitaet steht. Immer synchron zu
    /// world->tiles[y][x].entity -- beide Seiten werden ausschliesslich von
    /// RAD_WorldSpawnEntity, RAD_WorldMoveEntity und RAD_WorldRemoveEntity
    /// fortgeschrieben.
    ///
    int32_t x;
    int32_t y;
} RAD_Entity_t;


#endif
