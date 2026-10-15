#include "common.h"

///
/// Boden: sandbraun.
///
#define RAD_ISO_TILE_GROUND_TOP_COLOR ((SDL_Color){ 181, 155, 110, 255 })

void RAD_IsoTileInitGround(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_GROUND_TOP_COLOR;
    object->image = NULL;
    object->background = RAD_ISO_DECORATION_NONE;
    object->foreground = RAD_ISO_DECORATION_NONE;
}
