#include "common.h"

///
/// Strasse: asphaltgrau.
///
#define RAD_ISO_TILE_ROAD_TOP_COLOR ((SDL_Color){ 85, 85, 90, 255 })

void RAD_IsoTileInitRoad(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_ROAD_TOP_COLOR;
    object->image = NULL;
    object->overlay = NULL;
    object->overlay_offset_y = 0;
}
