#include <radish/rendering/iso_effect.h>
#include <radish/rendering/iso_definitions.h>

void RAD_RenderIsoEffect(SDL_Renderer *renderer, const RAD_IsoEffect_t *effect, int32_t screen_x, int32_t screen_y)
{
    SDL_Texture *texture = RAD_IsoEffectAtlasTexture(effect->sprite, effect->facing);
    const int32_t frame = RAD_IsoEffectFrameAt(effect->sprite, effect->facing, effect->progress);
    SDL_Rect source, target;
    if((texture != NULL) && RAD_IsoEffectFrameSource(effect->sprite, effect->facing, frame, &source)
       && RAD_IsoEffectFrameTarget(effect->sprite, effect->facing, screen_x + RAD_ISO_TILE_WIDTH / 2,
                                   screen_y + RAD_ISO_TILE_HEIGHT / 2, &target))
    {
        SDL_RenderCopy(renderer, texture, &source, &target);
    }
}
