#include "common.h"

///
/// Befestigte Stellung: oliv.
///
#define RAD_ISO_TILE_FORTIFICATION_TOP_COLOR ((SDL_Color){ 95, 105, 60, 255 })

///
/// Ein Wall aus Sandsaecken relativ zur Raute des Feldes
/// (RAD_IsoTileDrawSandbagWall).
///
typedef struct
{
    int32_t x0;
    int32_t y0;
    int32_t x1;
    int32_t y1;
    int32_t height;
} RAD_IsoTileFortificationWall_t;

///
/// Zwei Waelle knapp innerhalb der vorderen Kanten, unten eine Luecke als
/// Zugang. Im Vordergrund: sie verdecken den unteren Rand der Einheit, die damit
/// sichtbar in Deckung steht.
///
static const RAD_IsoTileFortificationWall_t RAD_ISO_TILE_FORTIFICATION_WALLS[] = {
    { .x0 =  5, .y0 = 16, .x1 = 27, .y1 = 27, .height = 6 },
    { .x0 = 33, .y0 = 27, .x1 = 55, .y1 = 16, .height = 6 }
};

static const RAD_IsoTileLayout_t RAD_ISO_TILE_FORTIFICATION_WALL_LAYOUT = {
    .entries = RAD_ISO_TILE_FORTIFICATION_WALLS,
    .number_of_entries = sizeof(RAD_ISO_TILE_FORTIFICATION_WALLS) / sizeof(RAD_ISO_TILE_FORTIFICATION_WALLS[0])
};

static void RAD_IsoTileRenderFortificationWalls(SDL_Renderer *renderer, const void *user_argument,
                                                int32_t screen_x, int32_t screen_y)
{
    const RAD_IsoTileLayout_t *layout = (const RAD_IsoTileLayout_t*)user_argument;
    const RAD_IsoTileFortificationWall_t *walls = (const RAD_IsoTileFortificationWall_t*)layout->entries;

    for(int32_t i=0;i < layout->number_of_entries; ++i)
    {
        RAD_IsoTileDrawSandbagWall(renderer,
                                   screen_x + walls[i].x0, screen_y + walls[i].y0,
                                   screen_x + walls[i].x1, screen_y + walls[i].y1,
                                   walls[i].height);
    }
}

void RAD_IsoTileInitFortification(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_FORTIFICATION_TOP_COLOR;
    object->image = NULL;
    object->background = RAD_ISO_DECORATION_NONE;
    object->foreground = (RAD_IsoDecoration_t){
        .render = RAD_IsoTileRenderFortificationWalls,
        .user_argument = &RAD_ISO_TILE_FORTIFICATION_WALL_LAYOUT
    };
}
