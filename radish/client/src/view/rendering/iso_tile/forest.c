#include "common.h"

///
/// Wald: dunkelgruen, wenn die Bilder fehlen. Bild und Dekoration
/// stehen in assets/tiles/forest/tile.json (view/rendering/iso_tile_images.h).
///
#define RAD_ISO_TILE_FOREST_TOP_COLOR ((SDL_Color){ 40, 110, 50, 255 })

void RAD_IsoTileInitForest(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_FOREST_TOP_COLOR;
}
