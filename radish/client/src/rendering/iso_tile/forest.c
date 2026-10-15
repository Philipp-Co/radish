#include "common.h"

///
/// Wald: dunkelgruen.
///
#define RAD_ISO_TILE_FOREST_TOP_COLOR ((SDL_Color){ 40, 110, 50, 255 })

///
/// Eine Tanne relativ zur Raute des Feldes (RAD_IsoTileDrawTree).
///
typedef struct
{
    int32_t x;
    int32_t y;
    int32_t height;
    int32_t width;
} RAD_IsoTileForestTree_t;

///
/// Ein Grasbueschel relativ zur Raute des Feldes (RAD_IsoTileDrawTuft).
///
typedef struct
{
    int32_t x;
    int32_t y;
} RAD_IsoTileForestTuft_t;

///
/// Drei Tannen auf der hinteren Haelfte der Raute, von hinten nach vorne. Hoeher
/// als ein Feld: eine Einheit verdeckt die hintere Haelfte, darueber bleiben die
/// Spitzen zu sehen.
///
static const RAD_IsoTileForestTree_t RAD_ISO_TILE_FOREST_TREES[] = {
    { .x = 30, .y =  6, .height = 34, .width = 16 },
    { .x = 16, .y = 12, .height = 28, .width = 14 },
    { .x = 44, .y = 12, .height = 30, .width = 14 }
};

///
/// Vier Bueschel an der vorderen Kante, von hinten nach vorne -- unterhalb der
/// Einheit, die bei y = RAD_ISO_TILE_HEIGHT / 2 endet.
///
static const RAD_IsoTileForestTuft_t RAD_ISO_TILE_FOREST_TUFTS[] = {
    { .x = 14, .y = 19 },
    { .x = 46, .y = 19 },
    { .x = 24, .y = 25 },
    { .x = 36, .y = 25 }
};

static const RAD_IsoTileLayout_t RAD_ISO_TILE_FOREST_TREE_LAYOUT = {
    .entries = RAD_ISO_TILE_FOREST_TREES,
    .number_of_entries = sizeof(RAD_ISO_TILE_FOREST_TREES) / sizeof(RAD_ISO_TILE_FOREST_TREES[0])
};

static const RAD_IsoTileLayout_t RAD_ISO_TILE_FOREST_TUFT_LAYOUT = {
    .entries = RAD_ISO_TILE_FOREST_TUFTS,
    .number_of_entries = sizeof(RAD_ISO_TILE_FOREST_TUFTS) / sizeof(RAD_ISO_TILE_FOREST_TUFTS[0])
};

static void RAD_IsoTileRenderForestTrees(SDL_Renderer *renderer, const void *user_argument,
                                         int32_t screen_x, int32_t screen_y)
{
    const RAD_IsoTileLayout_t *layout = (const RAD_IsoTileLayout_t*)user_argument;
    const RAD_IsoTileForestTree_t *trees = (const RAD_IsoTileForestTree_t*)layout->entries;

    for(int32_t i=0;i < layout->number_of_entries; ++i)
    {
        RAD_IsoTileDrawTree(renderer, screen_x + trees[i].x, screen_y + trees[i].y, trees[i].height, trees[i].width);
    }
}

static void RAD_IsoTileRenderForestScrub(SDL_Renderer *renderer, const void *user_argument,
                                         int32_t screen_x, int32_t screen_y)
{
    const RAD_IsoTileLayout_t *layout = (const RAD_IsoTileLayout_t*)user_argument;
    const RAD_IsoTileForestTuft_t *tufts = (const RAD_IsoTileForestTuft_t*)layout->entries;

    for(int32_t i=0;i < layout->number_of_entries; ++i)
    {
        RAD_IsoTileDrawTuft(renderer, screen_x + tufts[i].x, screen_y + tufts[i].y);
    }
}

void RAD_IsoTileInitForest(RAD_IsoObject_t *object)
{
    object->background_color = RAD_ISO_TILE_FOREST_TOP_COLOR;
    object->image = NULL;
    object->background = (RAD_IsoDecoration_t){
        .render = RAD_IsoTileRenderForestTrees,
        .user_argument = &RAD_ISO_TILE_FOREST_TREE_LAYOUT
    };
    object->foreground = (RAD_IsoDecoration_t){
        .render = RAD_IsoTileRenderForestScrub,
        .user_argument = &RAD_ISO_TILE_FOREST_TUFT_LAYOUT
    };
}
