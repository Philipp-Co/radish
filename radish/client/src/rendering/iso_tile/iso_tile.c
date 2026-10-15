#include "common.h"

///
/// Ohne default: kommt ein Tile-Typ dazu, meldet -Wswitch hier, dass er noch
/// keine eigene Funktion hat.
///
void RAD_IsoTileApplyType(RAD_IsoObject_t *object, RAD_ClientTileType_t type)
{
    switch(type)
    {
        case RAD_CLIENT_TILE_TYPE_VOID:          RAD_IsoTileInitVoid(object);          return;
        case RAD_CLIENT_TILE_TYPE_GROUND:        RAD_IsoTileInitGround(object);        return;
        case RAD_CLIENT_TILE_TYPE_WATER_SHALLOW: RAD_IsoTileInitWaterShallow(object);  return;
        case RAD_CLIENT_TILE_TYPE_MUD:           RAD_IsoTileInitMud(object);           return;
        case RAD_CLIENT_TILE_TYPE_MEADOW:        RAD_IsoTileInitMeadow(object);        return;
        case RAD_CLIENT_TILE_TYPE_FOREST:        RAD_IsoTileInitForest(object);        return;
        case RAD_CLIENT_TILE_TYPE_WATER_DEEP:    RAD_IsoTileInitWaterDeep(object);     return;
        case RAD_CLIENT_TILE_TYPE_CITY:          RAD_IsoTileInitCity(object);          return;
        case RAD_CLIENT_TILE_TYPE_ROAD:          RAD_IsoTileInitRoad(object);          return;
        case RAD_CLIENT_TILE_TYPE_FORTIFICATION: RAD_IsoTileInitFortification(object); return;
        case RAD_CLIENT_TILE_TYPE_UNKNOWN:       RAD_IsoTileInitUnknown(object);       return;
    }

    // Ein Wert ausserhalb der Aufzaehlung.
    RAD_IsoTileInitUnknown(object);
}
