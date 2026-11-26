#include "common.h"

///
/// Drachenzaehne: grau, wenn die Bilder fehlen. Bild und Dekoration
/// stehen in assets/tiles/dragons_teeth/tile.json (view/rendering/iso_tile_images.h).
///
#define RAD_ISO_TILE_DRAGONS_TEETH_TOP_COLOR ((SDL_Color){ 150, 150, 140, 255 })

void RAD_IsoTileInitDragonsTeeth(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_DRAGONS_TEETH_TOP_COLOR;
}
