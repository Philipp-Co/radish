#include <radish/control/terrain.h>


const char* RAD_ControlTerrainName(const RAD_ClientTile_t *tile)
{
    switch(tile->type)
    {
        case RAD_CLIENT_TILE_TYPE_GROUND:        return "Boden";
        case RAD_CLIENT_TILE_TYPE_WATER_SHALLOW: return "Seichtes Wasser";
        case RAD_CLIENT_TILE_TYPE_VOID:          return "Leere";
        case RAD_CLIENT_TILE_TYPE_MUD:           return "Matsch";
        case RAD_CLIENT_TILE_TYPE_MEADOW:        return "Wiese";
        case RAD_CLIENT_TILE_TYPE_FOREST:        return "Wald";
        case RAD_CLIENT_TILE_TYPE_WATER_DEEP:    return "Tiefes Wasser";
        case RAD_CLIENT_TILE_TYPE_CITY:          return "Stadt";
        case RAD_CLIENT_TILE_TYPE_ROAD:          return "Strasse";
        case RAD_CLIENT_TILE_TYPE_FORTIFICATION: return "Befestigte Stellung";
        case RAD_CLIENT_TILE_TYPE_UNKNOWN:       return "Unbekannt";
    }
    return "Unbekannt";
}

int32_t RAD_ControlTerrainMovementModifier(const RAD_ClientTile_t *tile)
{
    return tile->terrain.movement_modifier;
}

int32_t RAD_ControlTerrainMovementCost(const RAD_ClientTile_t *tile)
{
    // Ein Feld, das die Reichweite erhoeht, ist nicht umsonst.
    const int32_t cost = 1 - tile->terrain.movement_modifier;
    return (cost > 1) ? cost : 1;
}

int32_t RAD_ControlTerrainCover(const RAD_ClientTile_t *tile)
{
    return tile->terrain.cover;
}

bool RAD_ControlTerrainImpassable(const RAD_ClientTile_t *tile)
{
    // Ohne Gelaende auch dann nicht, wenn der Server es nicht als unbegehbar
    // fuehrt -- er lehnt den Weg trotzdem ab (RAD_GameCheckMovePath).
    return tile->terrain.impassable || (tile->type == RAD_CLIENT_TILE_TYPE_VOID);
}
