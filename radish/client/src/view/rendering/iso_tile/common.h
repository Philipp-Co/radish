#ifndef __RAD_VIEW_RENDERING_ISO_TILE_COMMON_H__
#define __RAD_VIEW_RENDERING_ISO_TILE_COMMON_H__

#include <radish/view/rendering/iso_tile.h>

///
/// Die Einrichtung je Typ, eine Datei je Funktion (view/rendering/iso_tile/). Nicht
/// unter include/: aufgerufen wird nur aus RAD_IsoTileApplyType. Jede setzt nur
/// die Farbe des Wuerfels -- Bild und Dekoration kommen aus der tile.json des
/// Typs (view/rendering/iso_tile_images.h).
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
void RAD_IsoTileInitDragonsTeeth(RAD_IsoObject_t *object);
void RAD_IsoTileInitHighGrass(RAD_IsoObject_t *object);
void RAD_IsoTileInitUnknown(RAD_IsoObject_t *object);

#endif
