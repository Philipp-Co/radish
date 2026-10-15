#include "common.h"

///
/// Stadt: grau wie Pflaster, heller als die Strasse; das Rot tragen die Daecher.
///
#define RAD_ISO_TILE_CITY_TOP_COLOR ((SDL_Color){ 150, 150, 145, 255 })

///
/// Ein Haus relativ zur Raute des Feldes (RAD_IsoTileDrawHouse).
///
typedef struct
{
    int32_t x;
    int32_t y;
    int32_t half_width;
    int32_t height;
    int32_t roof_height;
} RAD_IsoTileCityHouse_t;

///
/// Drei Haeuser auf der hinteren Haelfte der Raute, von hinten nach vorne. Wie
/// die Baeume im Wald hoeher als die Einheit, damit die Daecher ueber ihr zu
/// sehen bleiben.
///
static const RAD_IsoTileCityHouse_t RAD_ISO_TILE_CITY_HOUSES[] = {
    { .x = 30, .y =  8, .half_width = 9, .height = 14, .roof_height = 7 },
    { .x = 16, .y = 14, .half_width = 8, .height = 10, .roof_height = 6 },
    { .x = 44, .y = 14, .half_width = 8, .height = 11, .roof_height = 6 }
};

static const RAD_IsoTileLayout_t RAD_ISO_TILE_CITY_HOUSE_LAYOUT = {
    .entries = RAD_ISO_TILE_CITY_HOUSES,
    .number_of_entries = sizeof(RAD_ISO_TILE_CITY_HOUSES) / sizeof(RAD_ISO_TILE_CITY_HOUSES[0])
};

static void RAD_IsoTileRenderCityHouses(SDL_Renderer *renderer, const void *user_argument,
                                        int32_t screen_x, int32_t screen_y)
{
    const RAD_IsoTileLayout_t *layout = (const RAD_IsoTileLayout_t*)user_argument;
    const RAD_IsoTileCityHouse_t *houses = (const RAD_IsoTileCityHouse_t*)layout->entries;

    for(int32_t i=0;i < layout->number_of_entries; ++i)
    {
        RAD_IsoTileDrawHouse(renderer, screen_x + houses[i].x, screen_y + houses[i].y,
                             houses[i].half_width, houses[i].height, houses[i].roof_height);
    }
}

void RAD_IsoTileInitCity(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_CITY_TOP_COLOR;
    object->image = NULL;
    object->background = (RAD_IsoDecoration_t){
        .render = RAD_IsoTileRenderCityHouses,
        .user_argument = &RAD_ISO_TILE_CITY_HOUSE_LAYOUT
    };
    object->foreground = RAD_ISO_DECORATION_NONE;
}
