#include <radish/rendering/entity.h>
#include <radish/rendering/iso_definitions.h>

void RAD_RenderIsoEntity(SDL_Renderer *renderer, const RAD_IsoEntity_t *entity, int32_t screen_x, int32_t screen_y)
{
    (void)entity;

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect rect = {.x=screen_x,.y=screen_y-RAD_ISO_TILE_HEIGHT/2,.w=RAD_ISO_TILE_WIDTH,.h=RAD_ISO_TILE_HEIGHT};
    SDL_RenderFillRect(renderer, &rect);
}
