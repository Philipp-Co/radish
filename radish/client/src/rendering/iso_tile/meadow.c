#include "common.h"
#include <radish/rendering/iso_tile_images.h>

///
/// Wiese: hellgruen.
///
#define RAD_ISO_TILE_MEADOW_TOP_COLOR ((SDL_Color){ 130, 190, 90, 255 })

///
/// Ein Grashalm relativ zur Raute des Feldes (RAD_IsoTileDrawBlade).
///
typedef struct
{
    int32_t x;
    int32_t y;
    int32_t height;
    int32_t lean;
} RAD_IsoTileMeadowBlade_t;

///
/// Fuenf Paare Halme, ueber die Raute verstreut, von hinten nach vorne. Im
/// Hintergrund: sie sind so niedrig, dass eine Einheit sie ruhig verdecken darf.
///
static const RAD_IsoTileMeadowBlade_t RAD_ISO_TILE_MEADOW_BLADES[] = {
    { .x = 27, .y =  8, .height = 5, .lean = -1 },
    { .x = 29, .y =  9, .height = 4, .lean =  1 },
    { .x = 44, .y = 12, .height = 4, .lean = -1 },
    { .x = 46, .y = 13, .height = 5, .lean =  1 },
    { .x = 13, .y = 15, .height = 5, .lean = -1 },
    { .x = 15, .y = 16, .height = 4, .lean =  1 },
    { .x = 40, .y = 21, .height = 4, .lean =  1 },
    { .x = 38, .y = 22, .height = 5, .lean = -1 },
    { .x = 22, .y = 22, .height = 4, .lean = -1 },
    { .x = 24, .y = 23, .height = 5, .lean =  1 }
};

static const RAD_IsoTileLayout_t RAD_ISO_TILE_MEADOW_BLADE_LAYOUT = {
    .entries = RAD_ISO_TILE_MEADOW_BLADES,
    .number_of_entries = sizeof(RAD_ISO_TILE_MEADOW_BLADES) / sizeof(RAD_ISO_TILE_MEADOW_BLADES[0])
};

static void RAD_IsoTileRenderMeadowBlades(SDL_Renderer *renderer, const void *user_argument,
                                          int32_t screen_x, int32_t screen_y)
{
    const RAD_IsoTileLayout_t *layout = (const RAD_IsoTileLayout_t*)user_argument;
    const RAD_IsoTileMeadowBlade_t *blades = (const RAD_IsoTileMeadowBlade_t*)layout->entries;

    for(int32_t i=0;i < layout->number_of_entries; ++i)
    {
        RAD_IsoTileDrawBlade(renderer, screen_x + blades[i].x, screen_y + blades[i].y,
                             blades[i].height, blades[i].lean);
    }
}

void RAD_IsoTileInitMeadow(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_MEADOW_TOP_COLOR;
    object->image = RAD_IsoTileImage(RAD_ISO_TILE_IMAGE_GRASS);
    object->background = (RAD_IsoDecoration_t){
        .render = RAD_IsoTileRenderMeadowBlades,
        .user_argument = &RAD_ISO_TILE_MEADOW_BLADE_LAYOUT
    };
    object->foreground = RAD_ISO_DECORATION_NONE;
}
