#include "common.h"

#define RAD_ISO_TILE_TREE_TRUNK_COLOR       ((SDL_Color){ 90, 60, 30, 255 })
#define RAD_ISO_TILE_TREE_LIGHT_COLOR       ((SDL_Color){ 30, 95, 40, 255 })
#define RAD_ISO_TILE_TREE_DARK_COLOR        ((SDL_Color){ 15, 60, 25, 255 })
#define RAD_ISO_TILE_TREE_TRUNK_WIDTH       3
#define RAD_ISO_TILE_TREE_TRUNK_HEIGHT      4

#define RAD_ISO_TILE_TUFT_LIGHT_COLOR       ((SDL_Color){ 85, 120, 45, 255 })
#define RAD_ISO_TILE_TUFT_DARK_COLOR        ((SDL_Color){ 60, 90, 30, 255 })

#define RAD_ISO_TILE_BLADE_COLOR            ((SDL_Color){ 85, 150, 55, 255 })

#define RAD_ISO_TILE_HOUSE_WALL_LIGHT_COLOR ((SDL_Color){ 225, 215, 195, 255 })
#define RAD_ISO_TILE_HOUSE_WALL_DARK_COLOR  ((SDL_Color){ 175, 165, 150, 255 })
#define RAD_ISO_TILE_HOUSE_ROOF_LIGHT_COLOR ((SDL_Color){ 180, 70, 50, 255 })
#define RAD_ISO_TILE_HOUSE_ROOF_DARK_COLOR  ((SDL_Color){ 130, 45, 35, 255 })

#define RAD_ISO_TILE_SANDBAG_COLOR          ((SDL_Color){ 190, 170, 120, 255 })
#define RAD_ISO_TILE_SANDBAG_JOINT_COLOR    ((SDL_Color){ 120, 105, 70, 255 })
/// Wie lang ein Sack ist, in Pixeln entlang des Walls.
#define RAD_ISO_TILE_SANDBAG_LENGTH         7

///
/// Ein gefuelltes Dreieck in einer Farbe.
///
static void RAD_IsoTileFillTriangle(SDL_Renderer *renderer, SDL_Color color,
                                    int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t x2, int32_t y2)
{
    const SDL_Vertex vertices[3] = {
        { .position = { .x = x0, .y = y0 }, .color = color, .tex_coord = { .x = 0.0, .y = 0.0 } },
        { .position = { .x = x1, .y = y1 }, .color = color, .tex_coord = { .x = 0.0, .y = 0.0 } },
        { .position = { .x = x2, .y = y2 }, .color = color, .tex_coord = { .x = 0.0, .y = 0.0 } }
    };
    SDL_RenderGeometry(renderer, NULL, vertices, 3, NULL, 0);
}

///
/// Ein gefuelltes Viereck (x0, y0) .. (x3, y3) in einer Farbe, die Ecken der
/// Reihe nach.
///
static void RAD_IsoTileFillQuad(SDL_Renderer *renderer, SDL_Color color,
                                int32_t x0, int32_t y0, int32_t x1, int32_t y1,
                                int32_t x2, int32_t y2, int32_t x3, int32_t y3)
{
    RAD_IsoTileFillTriangle(renderer, color, x0, y0, x1, y1, x2, y2);
    RAD_IsoTileFillTriangle(renderer, color, x0, y0, x2, y2, x3, y3);
}

void RAD_IsoTileDrawTree(SDL_Renderer *renderer, int32_t x, int32_t y, int32_t height, int32_t width)
{
    const SDL_Color trunk = RAD_ISO_TILE_TREE_TRUNK_COLOR;
    SDL_SetRenderDrawColor(renderer, trunk.r, trunk.g, trunk.b, trunk.a);
    const SDL_Rect rect = {
        .x = x - RAD_ISO_TILE_TREE_TRUNK_WIDTH / 2,
        .y = y,
        .w = RAD_ISO_TILE_TREE_TRUNK_WIDTH,
        .h = RAD_ISO_TILE_TREE_TRUNK_HEIGHT
    };
    SDL_RenderFillRect(renderer, &rect);

    // Die Krone, an der Mittelachse geteilt: wie von links beleuchtet.
    RAD_IsoTileFillTriangle(renderer, RAD_ISO_TILE_TREE_LIGHT_COLOR, x - width / 2, y, x, y - height, x, y);
    RAD_IsoTileFillTriangle(renderer, RAD_ISO_TILE_TREE_DARK_COLOR,  x, y - height, x + width / 2, y, x, y);
}

void RAD_IsoTileDrawTuft(SDL_Renderer *renderer, int32_t x, int32_t y)
{
    RAD_IsoTileFillTriangle(renderer, RAD_ISO_TILE_TUFT_DARK_COLOR,  x - 6, y, x - 5, y - 4, x - 2, y);
    RAD_IsoTileFillTriangle(renderer, RAD_ISO_TILE_TUFT_DARK_COLOR,  x + 2, y, x + 5, y - 4, x + 6, y);
    RAD_IsoTileFillTriangle(renderer, RAD_ISO_TILE_TUFT_LIGHT_COLOR, x - 3, y, x,     y - 7, x + 3, y);
}

void RAD_IsoTileDrawBlade(SDL_Renderer *renderer, int32_t x, int32_t y, int32_t height, int32_t lean)
{
    RAD_IsoTileFillTriangle(renderer, RAD_ISO_TILE_BLADE_COLOR, x - 1, y, x + lean, y - height, x + 1, y);
}

void RAD_IsoTileDrawHouse(SDL_Renderer *renderer, int32_t x, int32_t y,
                          int32_t half_width, int32_t height, int32_t roof_height)
{
    const int32_t a = half_width;
    const int32_t b = half_width / 2;
    const int32_t apex_y = y - height - roof_height;

    // Die beiden sichtbaren Waende: links hell, rechts dunkel, wie die Baeume.
    RAD_IsoTileFillQuad(renderer, RAD_ISO_TILE_HOUSE_WALL_LIGHT_COLOR,
                        x - a, y, x, y + b, x, y + b - height, x - a, y - height);
    RAD_IsoTileFillQuad(renderer, RAD_ISO_TILE_HOUSE_WALL_DARK_COLOR,
                        x, y + b, x + a, y, x + a, y - height, x, y + b - height);

    // Das Dach: die hinteren Flaechen zuerst, die vorderen darueber.
    RAD_IsoTileFillTriangle(renderer, RAD_ISO_TILE_HOUSE_ROOF_DARK_COLOR,
                            x - a, y - height, x, y - b - height, x, apex_y);
    RAD_IsoTileFillTriangle(renderer, RAD_ISO_TILE_HOUSE_ROOF_DARK_COLOR,
                            x, y - b - height, x + a, y - height, x, apex_y);
    RAD_IsoTileFillTriangle(renderer, RAD_ISO_TILE_HOUSE_ROOF_LIGHT_COLOR,
                            x - a, y - height, x, y + b - height, x, apex_y);
    RAD_IsoTileFillTriangle(renderer, RAD_ISO_TILE_HOUSE_ROOF_DARK_COLOR,
                            x, y + b - height, x + a, y - height, x, apex_y);
}

void RAD_IsoTileDrawSandbagWall(SDL_Renderer *renderer, int32_t x0, int32_t y0,
                                int32_t x1, int32_t y1, int32_t height)
{
    RAD_IsoTileFillQuad(renderer, RAD_ISO_TILE_SANDBAG_COLOR,
                        x0, y0, x1, y1, x1, y1 - height, x0, y0 - height);

    // Die Fugen: Ober- und Unterkante und alle RAD_ISO_TILE_SANDBAG_LENGTH Pixel
    // (entlang x gemessen) ein Strich von unten nach oben.
    const SDL_Color joint = RAD_ISO_TILE_SANDBAG_JOINT_COLOR;
    SDL_SetRenderDrawColor(renderer, joint.r, joint.g, joint.b, joint.a);
    SDL_RenderDrawLine(renderer, x0, y0, x1, y1);
    SDL_RenderDrawLine(renderer, x0, y0 - height, x1, y1 - height);

    const int32_t dx = x1 - x0;
    const int32_t length = (dx < 0) ? -dx : dx;
    const int32_t number_of_bags = (length + RAD_ISO_TILE_SANDBAG_LENGTH / 2) / RAD_ISO_TILE_SANDBAG_LENGTH;
    for(int32_t i=1;i < number_of_bags; ++i)
    {
        const int32_t jx = x0 + dx * i / number_of_bags;
        const int32_t jy = y0 + (y1 - y0) * i / number_of_bags;
        SDL_RenderDrawLine(renderer, jx, jy, jx, jy - height);
    }
}
