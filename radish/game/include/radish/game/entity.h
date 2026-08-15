#ifndef __RAD_GAME_ENTITY_H__
#define __RAD_GAME_ENTITY_H__

#include <stdint.h>
#include <radish/game/user.h>

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
    /// Wem die Entitaet gehoert, RAD_USER_NONE fuer herrenlos. Sie steht damit in
    /// der Entitaet und nicht in einer Liste je Benutzer -- die Id ist der
    /// Slot-Index und wird nach einer Loeschung wiederverwendet, und eine Liste
    /// daneben muesste bei jedem Entfernen mitgezogen werden. Wird sie das einmal
    /// nicht, erbt die naechste Figur in diesem Slot still den alten Besitzer.
    /// Hier kann das nicht passieren: RAD_WorldSpawnEntityWithId schreibt den
    /// ganzen Platz, RAD_WorldRemoveEntity raeumt ihn.
    ///
    /// Herrenlos ist der Normalfall und kein Fehler -- alles, was nicht
    /// ausdruecklich zugeordnet wurde, gehoert niemandem.
    ///
    RAD_UserId_t owner;

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
