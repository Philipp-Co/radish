#include <radish/view/rendering/iso_effect.h>
#include <radish/view/rendering/iso_definitions.h>

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


void RAD_IsoEffectStart(RAD_IsoEffect_t *effect, RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing,
                        float x, float y, float start_ms)
{
    *effect = (RAD_IsoEffect_t){
        .sprite = sprite,
        .facing = facing,
        .x = x,
        .y = y,
        .progress = 0.0f,
        .active = true,
        .start_ms = start_ms
    };
}

bool RAD_IsoEffectUpdate(RAD_IsoEffect_t *effect, float now_ms)
{
    if(!RAD_IsoEffectHasStarted(effect, now_ms))
    {
        return true;
    }

    // Ohne Bild gibt es keine Dauer; dann ist der Effekt sofort durch.
    const float duration_ms = RAD_IsoEffectDurationMs(effect->sprite);
    const float progress = (duration_ms > 0.0f) ? (now_ms - effect->start_ms) / duration_ms : 1.0f;
    if(progress >= 1.0f)
    {
        effect->active = false;
        return false;
    }

    effect->progress = progress;
    return true;
}

bool RAD_IsoEffectHasStarted(const RAD_IsoEffect_t *effect, float now_ms)
{
    return effect->active && (now_ms >= effect->start_ms);
}
