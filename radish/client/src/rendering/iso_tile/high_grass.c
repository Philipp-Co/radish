#include "common.h"
#include <radish/rendering/iso_tile_images.h>

///
/// Hohes Gras: kraeftig gruen, wenn die Bilder fehlen.
///
#define RAD_ISO_TILE_HIGH_GRASS_TOP_COLOR ((SDL_Color){ 70, 130, 45, 255 })

///
/// Beide Haelften stehen im selben Rahmen, zusammen ein Grasbuschel. Seine
/// Grundflaeche hat die Mitte bei (61, 65) statt bei (64, 32) wie die Oberseite
/// des Rasenwuerfels -- doppelt so gross gezeichnet (dragons_teeth.c).
///
#define RAD_ISO_TILE_HIGH_GRASS_OFFSET_X (2 * (64 - 61))
#define RAD_ISO_TILE_HIGH_GRASS_OFFSET_Y (2 * (32 - 65))

///
/// Die hintere Haelfte vor, die vordere nach den Figuren: eine Einheit steht
/// im Gras.
///
void RAD_IsoTileInitHighGrass(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_HIGH_GRASS_TOP_COLOR;
    object->image = RAD_IsoTileImage(RAD_ISO_TILE_IMAGE_GRASS);
    object->overlay = RAD_IsoTileImage(RAD_ISO_TILE_IMAGE_HIGH_GRASS_BACK);
    object->overlay_offset_x = RAD_ISO_TILE_HIGH_GRASS_OFFSET_X;
    object->overlay_offset_y = RAD_ISO_TILE_HIGH_GRASS_OFFSET_Y;
    object->overlay_front = RAD_IsoTileImage(RAD_ISO_TILE_IMAGE_HIGH_GRASS_FRONT);
}
