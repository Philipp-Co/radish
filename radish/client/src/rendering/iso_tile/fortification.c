#include "common.h"

///
/// Befestigte Stellung: oliv.
///
#define RAD_ISO_TILE_FORTIFICATION_TOP_COLOR ((SDL_Color){ 95, 105, 60, 255 })

void RAD_IsoTileInitFortification(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_FORTIFICATION_TOP_COLOR;
    object->image = NULL;
    object->overlay = NULL;
    object->overlay_offset_y = 0;
}
