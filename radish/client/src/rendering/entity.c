#include <radish/rendering/entity.h>
#include <radish/rendering/iso_definitions.h>
#include <math.h>

///
/// Der Schatten unter einer Einheit mit Bild: eine flache Ellipse, doppelt so
/// breit wie hoch wie die Raute, auf deren Mitte und etwas darunter. Groesser
/// als die Grundflaeche des Fahrzeugs, sonst verdeckt es ihn ganz -- zu sehen
/// ist nur der Saum um die Ketten. Bis RAD_ISO_SHADOW_CORE seines Radius voll
/// RAD_ISO_SHADOW_ALPHA, danach nach aussen auslaufend.
///
#define RAD_ISO_SHADOW_RADIUS_X 100
#define RAD_ISO_SHADOW_RADIUS_Y 50
#define RAD_ISO_SHADOW_OFFSET_Y 4
#define RAD_ISO_SHADOW_ALPHA 150
#define RAD_ISO_SHADOW_CORE 0.55f
#define RAD_ISO_SHADOW_SEGMENTS 32

///
/// Ein Punkt auf der Ellipse des Schattens um (cx, cy), beim Winkel "angle" und
/// beim Anteil "scale" des Radius.
///
static SDL_Vertex RAD_IsoShadowVertex(float cx, float cy, float angle, float scale, SDL_Color color)
{
    return (SDL_Vertex){
        .position = {
            cx + cosf(angle) * RAD_ISO_SHADOW_RADIUS_X * scale,
            cy + sinf(angle) * RAD_ISO_SHADOW_RADIUS_Y * scale
        },
        .color = color
    };
}

///
/// Zeichnet den Schatten auf ein Feld, dessen Raute bei (screen_x, screen_y)
/// beginnt. Je Segment ein Dreieck von der Mitte zum Kern, beide Ecken voll
/// dunkel, und zwei Dreiecke vom Kern zum Rand, wo die Farbe auf 0 auslaeuft --
/// das gibt den weichen Rand ohne eigenes Bild.
///
static void RAD_RenderIsoEntityShadow(SDL_Renderer *renderer, int32_t screen_x, int32_t screen_y)
{
    const float cx = (float)(screen_x + RAD_ISO_TILE_WIDTH / 2);
    const float cy = (float)(screen_y + RAD_ISO_TILE_HEIGHT / 2 + RAD_ISO_SHADOW_OFFSET_Y);
    const SDL_Color inner = { 0, 0, 0, RAD_ISO_SHADOW_ALPHA };
    const SDL_Color outer = { 0, 0, 0, 0 };

    SDL_Vertex vertices[RAD_ISO_SHADOW_SEGMENTS * 9];
    for(int32_t i = 0; i < RAD_ISO_SHADOW_SEGMENTS; ++i)
    {
        const float a0 = 2.0f * (float)M_PI * (float)i / RAD_ISO_SHADOW_SEGMENTS;
        const float a1 = 2.0f * (float)M_PI * (float)(i + 1) / RAD_ISO_SHADOW_SEGMENTS;
        const SDL_Vertex core0 = RAD_IsoShadowVertex(cx, cy, a0, RAD_ISO_SHADOW_CORE, inner);
        const SDL_Vertex core1 = RAD_IsoShadowVertex(cx, cy, a1, RAD_ISO_SHADOW_CORE, inner);
        const SDL_Vertex rim0 = RAD_IsoShadowVertex(cx, cy, a0, 1.0f, outer);
        const SDL_Vertex rim1 = RAD_IsoShadowVertex(cx, cy, a1, 1.0f, outer);
        SDL_Vertex *v = &vertices[i * 9];

        v[0] = (SDL_Vertex){ .position = { cx, cy }, .color = inner };
        v[1] = core0;
        v[2] = core1;

        v[3] = core0;
        v[4] = rim0;
        v[5] = rim1;

        v[6] = core0;
        v[7] = rim1;
        v[8] = core1;
    }

    // Ohne Textur mischt SDL_RenderGeometry nach dem Modus des Renderers; den
    // des Aufrufers danach wiederherstellen.
    SDL_BlendMode previous = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &previous);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(renderer, NULL, vertices, RAD_ISO_SHADOW_SEGMENTS * 9, NULL, 0);
    SDL_SetRenderDrawBlendMode(renderer, previous);
}

void RAD_RenderIsoEntity(SDL_Renderer *renderer, const RAD_IsoEntity_t *entity, int32_t screen_x, int32_t screen_y)
{
    // Mit Turm nach vorn das Bild der Drehung, auch im Stand: bei 0 Grad zeigen
    // beide Atlanten dasselbe.
    RAD_IsoUnitAtlas_t atlas = RAD_ISO_UNIT_ATLAS_TURN;
    int32_t frame = RAD_IsoUnitFacingFrame(entity->sprite, entity->facing);
    if(entity->turning)
    {
        frame = RAD_IsoUnitTurnFrame(entity->sprite, entity->facing, entity->turn_to, entity->turn_progress);
    }
    else if((entity->turret_angle != 0.0f) && RAD_IsoUnitHasTurret(entity->sprite))
    {
        atlas = RAD_IsoUnitTurretAtlas(entity->facing);
        frame = RAD_IsoUnitTurretFrame(entity->sprite, entity->facing, entity->turret_angle);
    }

    SDL_Texture *texture = RAD_IsoUnitAtlasTexture(entity->sprite, atlas);
    SDL_Rect source, target;
    // Der Anker des Bildes kommt auf die Mitte der Raute.
    if((texture != NULL) && RAD_IsoUnitFrameSource(entity->sprite, atlas, frame, &source)
       && RAD_IsoUnitFrameTarget(entity->sprite, screen_x + RAD_ISO_TILE_WIDTH / 2,
                                 screen_y + RAD_ISO_TILE_HEIGHT / 2, &target))
    {
        if(!RAD_IsoUnitHasOwnShadow(entity->sprite))
        {
            RAD_RenderIsoEntityShadow(renderer, screen_x, screen_y);
        }
        SDL_RenderCopy(renderer, texture, &source, &target);
        return;
    }

    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect rect = {.x=screen_x,.y=screen_y-RAD_ISO_TILE_HEIGHT/2,.w=RAD_ISO_TILE_WIDTH,.h=RAD_ISO_TILE_HEIGHT};
    SDL_RenderFillRect(renderer, &rect);
}
