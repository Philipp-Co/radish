#include "common.h"

///
/// Ein Typ, den der Client nicht kennt: magenta, damit er sofort auffaellt.
///
#define RAD_ISO_TILE_UNKNOWN_TOP_COLOR ((SDL_Color){ 255, 0, 255, 255 })

void RAD_IsoTileInitUnknown(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_UNKNOWN_TOP_COLOR;
    object->image = NULL;
    object->overlay = NULL;
    object->overlay_offset_y = 0;
}
