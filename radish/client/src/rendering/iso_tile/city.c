#include "common.h"

///
/// Stadt: grau wie Pflaster, heller als die Strasse.
///
#define RAD_ISO_TILE_CITY_TOP_COLOR ((SDL_Color){ 150, 150, 145, 255 })

void RAD_IsoTileInitCity(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_CITY_TOP_COLOR;
    object->image = NULL;
    object->overlay = NULL;
    object->overlay_offset_y = 0;
}
