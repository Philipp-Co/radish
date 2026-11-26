#include "common.h"

///
/// Wiese: hellgruen, wenn die Bilder fehlen. Bild und Dekoration
/// stehen in assets/tiles/meadow/tile.json (view/rendering/iso_tile_images.h).
///
#define RAD_ISO_TILE_MEADOW_TOP_COLOR ((SDL_Color){ 130, 190, 90, 255 })

void RAD_IsoTileInitMeadow(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_MEADOW_TOP_COLOR;
}
