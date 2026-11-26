#include "common.h"

///
/// Hohes Gras: kraeftig gruen, wenn die Bilder fehlen. Bild und Dekoration
/// stehen in assets/tiles/high_grass/tile.json (view/rendering/iso_tile_images.h).
///
#define RAD_ISO_TILE_HIGH_GRASS_TOP_COLOR ((SDL_Color){ 70, 130, 45, 255 })

void RAD_IsoTileInitHighGrass(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_HIGH_GRASS_TOP_COLOR;
}
