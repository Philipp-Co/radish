#ifndef __RAD_RENDERING_ISO_TILE_COMMON_H__
#define __RAD_RENDERING_ISO_TILE_COMMON_H__

#include <radish/rendering/iso_tile.h>

///
/// Die Einrichtung je Typ, eine Datei je Funktion (rendering/iso_tile/). Nicht
/// unter include/: aufgerufen wird nur aus RAD_IsoTileApplyType. Jede setzt
/// alles, was vom Typ abhaengt (rendering/iso_tile.h).
///
void RAD_IsoTileInitVoid(RAD_IsoObject_t *object);
void RAD_IsoTileInitGround(RAD_IsoObject_t *object);
void RAD_IsoTileInitWaterShallow(RAD_IsoObject_t *object);
void RAD_IsoTileInitMud(RAD_IsoObject_t *object);
void RAD_IsoTileInitMeadow(RAD_IsoObject_t *object);
void RAD_IsoTileInitForest(RAD_IsoObject_t *object);
void RAD_IsoTileInitWaterDeep(RAD_IsoObject_t *object);
void RAD_IsoTileInitCity(RAD_IsoObject_t *object);
void RAD_IsoTileInitRoad(RAD_IsoObject_t *object);
void RAD_IsoTileInitFortification(RAD_IsoObject_t *object);
void RAD_IsoTileInitUnknown(RAD_IsoObject_t *object);

#endif
