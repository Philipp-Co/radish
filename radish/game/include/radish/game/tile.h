#ifndef __RAD_TILE_H__
#define __RAD_TILE_H__

#include <stdint.h>
#include <radish/game/entity.h>

typedef enum
{
    RAD_TILE_TYPE_VOID = 0,
    RAD_TILE_TYPE_GROUND,
    RAD_TILE_TYPE_WATER
} RAD_TileType_t;

typedef struct
{
    ///
    /// Position in der Welt. Redundant zur Array-Position, aber noetig, sobald
    /// ein einzelnes RAD_Tile_t* weitergereicht wird -- genauso haelt es
    /// RAD_IsoObject_t auf der Rendering-Seite.
    ///
    int32_t x;
    int32_t y;
    int32_t z;

    RAD_TileType_t type;

    ///
    /// Entitaet auf diesem Tile, RAD_ENTITY_NONE wenn frei. Pro Tile kann es zu
    /// jedem Zeitpunkt hoechstens eine geben.
    ///
    RAD_EntityId_t entity;
} RAD_Tile_t;

#endif
