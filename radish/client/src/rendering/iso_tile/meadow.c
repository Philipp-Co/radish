#include "common.h"
#include <radish/rendering/iso_tile_images.h>

///
/// Wiese: hellgruen.
///
#define RAD_ISO_TILE_MEADOW_TOP_COLOR ((SDL_Color){ 130, 190, 90, 255 })

///
/// Das Gras steht im Bild auf einer Raute in der Mitte des Rahmens, eine halbe
/// Feldhoehe unter der Oberseite des Rasenwuerfels.
///
#define RAD_ISO_TILE_MEADOW_GRASS_OFFSET_Y (-RAD_ISO_TILE_HEIGHT / 2)

void RAD_IsoTileInitMeadow(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_MEADOW_TOP_COLOR;
    object->image = RAD_IsoTileImage(RAD_ISO_TILE_IMAGE_GRASS);
    object->overlay = RAD_IsoTileImage(RAD_ISO_TILE_IMAGE_GRASS_DECOR);
    object->overlay_offset_y = RAD_ISO_TILE_MEADOW_GRASS_OFFSET_Y;
}
