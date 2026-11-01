#include "common.h"

///
/// Tiefes Wasser: dunkelblau.
///
#define RAD_ISO_TILE_WATER_DEEP_TOP_COLOR ((SDL_Color){ 30, 70, 150, 255 })

void RAD_IsoTileInitWaterDeep(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_WATER_DEEP_TOP_COLOR;
    object->image = NULL;
    object->overlay = NULL;
    object->overlay_offset_y = 0;
}
