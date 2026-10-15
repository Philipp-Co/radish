#include "common.h"

///
/// Matschiger Boden: dunkelbraun.
///
#define RAD_ISO_TILE_MUD_TOP_COLOR ((SDL_Color){ 110, 85, 55, 255 })

void RAD_IsoTileInitMud(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_MUD_TOP_COLOR;
    object->image = NULL;
    object->background = RAD_ISO_DECORATION_NONE;
    object->foreground = RAD_ISO_DECORATION_NONE;
}
