#include <radish/control/terrain.h>

#include <stddef.h>


typedef struct
{
    const char *name;
    int32_t movement_modifier;
    int32_t cover;
} RAD_ControlTerrainValues_t;

///
/// Die Werte je Typ (model/tile.h). MV und CV sind Platzhalter: das Spiel
/// kennt noch keine (control/terrain.h).
///
static const RAD_ControlTerrainValues_t RAD_CONTROL_TERRAIN_VALUES[] = {
    [RAD_CLIENT_TILE_TYPE_UNKNOWN] = { .name = "Unbekannt", .movement_modifier = 0, .cover = 0 },
    [RAD_CLIENT_TILE_TYPE_GROUND]  = { .name = "Boden",     .movement_modifier = 0, .cover = 1 },
    [RAD_CLIENT_TILE_TYPE_WATER]   = { .name = "Wasser",    .movement_modifier = -1, .cover = 0 },
    [RAD_CLIENT_TILE_TYPE_VOID]    = { .name = "Leere",     .movement_modifier = 0, .cover = 0 }
};

static RAD_ControlTerrainValues_t RAD_ControlTerrainValuesOf(const RAD_ClientTile_t *tile)
{
    const size_t type = (size_t)tile->type;
    if(type >= sizeof(RAD_CONTROL_TERRAIN_VALUES) / sizeof(RAD_CONTROL_TERRAIN_VALUES[0]))
    {
        return RAD_CONTROL_TERRAIN_VALUES[RAD_CLIENT_TILE_TYPE_UNKNOWN];
    }
    return RAD_CONTROL_TERRAIN_VALUES[type];
}

const char* RAD_ControlTerrainName(const RAD_ClientTile_t *tile)
{
    return RAD_ControlTerrainValuesOf(tile).name;
}

int32_t RAD_ControlTerrainMovementModifier(const RAD_ClientTile_t *tile)
{
    return RAD_ControlTerrainValuesOf(tile).movement_modifier;
}

int32_t RAD_ControlTerrainMovementCost(const RAD_ClientTile_t *tile)
{
    // Ein Feld, das die Reichweite erhoeht, ist nicht umsonst.
    const int32_t cost = 1 - RAD_ControlTerrainValuesOf(tile).movement_modifier;
    return (cost > 1) ? cost : 1;
}

int32_t RAD_ControlTerrainCover(const RAD_ClientTile_t *tile)
{
    return RAD_ControlTerrainValuesOf(tile).cover;
}
