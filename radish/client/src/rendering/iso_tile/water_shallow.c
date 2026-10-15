#include "common.h"

///
/// Seichtes Wasser: hellblau.
///
#define RAD_ISO_TILE_WATER_SHALLOW_TOP_COLOR ((SDL_Color){ 110, 180, 220, 255 })

void RAD_IsoTileInitWaterShallow(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_WATER_SHALLOW_TOP_COLOR;
    object->image = NULL;
    object->background = RAD_ISO_DECORATION_NONE;
    object->foreground = RAD_ISO_DECORATION_NONE;
}
