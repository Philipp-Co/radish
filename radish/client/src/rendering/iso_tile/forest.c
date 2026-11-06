#include "common.h"
#include <radish/rendering/iso_tile_images.h>

///
/// Wald: dunkelgruen, wenn die Bilder fehlen.
///
#define RAD_ISO_TILE_FOREST_TOP_COLOR ((SDL_Color){ 40, 110, 50, 255 })

///
/// Der Wald steht im Bild auf der unteren Haelfte des Rahmens, eine ganze
/// Feldhoehe unter der Oberseite des Rasenwuerfels. Die Krone ragt dann ueber
/// das Feld hinaus.
///
#define RAD_ISO_TILE_FOREST_OFFSET_Y (-RAD_ISO_TILE_HEIGHT)

void RAD_IsoTileInitForest(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_FOREST_TOP_COLOR;
    object->image = RAD_IsoTileImage(RAD_ISO_TILE_IMAGE_GRASS);
    object->overlay = RAD_IsoTileImage(RAD_ISO_TILE_IMAGE_FOREST);
    object->overlay_offset_x = 0;
    object->overlay_offset_y = RAD_ISO_TILE_FOREST_OFFSET_Y;
    object->overlay_front = NULL;
}
