#include "common.h"

///
/// Kein Gelaende: fast schwarz.
///
#define RAD_ISO_TILE_VOID_TOP_COLOR ((SDL_Color){ 40, 40, 40, 255 })

void RAD_IsoTileInitVoid(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_VOID_TOP_COLOR;
    object->image = NULL;
    object->background = RAD_ISO_DECORATION_NONE;
    object->foreground = RAD_ISO_DECORATION_NONE;
}
