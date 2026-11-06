#include "common.h"
#include <radish/rendering/iso_tile_images.h>

///
/// Drachenzaehne: grau, wenn die Bilder fehlen.
///
#define RAD_ISO_TILE_DRAGONS_TEETH_TOP_COLOR ((SDL_Color){ 150, 150, 140, 255 })

///
/// Die Grundflaeche der Hoecker ist im Bild eine Raute wie die Oberseite des
/// Rasenwuerfels, aber mit der Mitte bei (60, 78) statt (64, 32) -- im Rahmen
/// doppelt so gross gezeichnet (RAD_ISO_TILE_WIDTH = 2 * 128). Verschoben um
/// den Unterschied steht sie genau auf dem Feld.
///
#define RAD_ISO_TILE_DRAGONS_TEETH_OFFSET_X (2 * (64 - 60))
#define RAD_ISO_TILE_DRAGONS_TEETH_OFFSET_Y (2 * (32 - 78))

void RAD_IsoTileInitDragonsTeeth(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_DRAGONS_TEETH_TOP_COLOR;
    object->image = RAD_IsoTileImage(RAD_ISO_TILE_IMAGE_GRASS);
    object->overlay = RAD_IsoTileImage(RAD_ISO_TILE_IMAGE_DRAGONS_TEETH);
    object->overlay_offset_x = RAD_ISO_TILE_DRAGONS_TEETH_OFFSET_X;
    object->overlay_offset_y = RAD_ISO_TILE_DRAGONS_TEETH_OFFSET_Y;
    object->overlay_front = NULL;
}
