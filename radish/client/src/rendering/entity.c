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

///
/// Der Ring unter einer Figur, in der Farbe ihres Besitzers: ein flacher
/// Ellipsenring auf der Mitte der Raute, so gestaucht wie sie (doppelt so breit
/// wie hoch) -- auf der Karte ein Kreis am Boden. Etwas kleiner als die Raute,
/// damit er auf benachbarten Feldern nicht stoert, aber groesser als der Kern des
/// Schattens, damit er um das Fahrzeug herum zu sehen ist.
///
#define RAD_ISO_RING_RADIUS_X 96
#define RAD_ISO_RING_RADIUS_Y 48
#define RAD_ISO_RING_THICKNESS 0.14f
#define RAD_ISO_RING_ALPHA 128
#define RAD_ISO_RING_SEGMENTS 48

///
/// Die Farbe des Rings je Stelle des Besitzers unter den Mitspielern
/// (RAD_IsoEntity_t.player): der erste blau, der zweite gelb.
///
static const SDL_Color RAD_ISO_RING_COLORS[] = {
    { 40, 110, 255, RAD_ISO_RING_ALPHA },
    { 255, 210, 0, RAD_ISO_RING_ALPHA }
};

///
/// Ein Punkt auf der Ellipse des Rings um (cx, cy), beim Winkel "angle" und
/// beim Anteil "scale" des Radius.
///
static SDL_Vertex RAD_IsoRingVertex(float cx, float cy, float angle, float scale, SDL_Color color)
{
    return (SDL_Vertex){
        .position = {
            cx + cosf(angle) * RAD_ISO_RING_RADIUS_X * scale,
            cy + sinf(angle) * RAD_ISO_RING_RADIUS_Y * scale
        },
        .color = color
    };
}

///
/// Zeichnet den Ring fuer den Besitzer "player" auf ein Feld, dessen Raute bei
/// (screen_x, screen_y) beginnt -- je Segment zwei Dreiecke zwischen innerem und
/// aeusserem Rand. Ohne bekannten Besitzer nichts.
///
static void RAD_RenderIsoEntityRing(SDL_Renderer *renderer, int32_t player, int32_t screen_x, int32_t screen_y)
{
    if((player < 0) || (player >= (int32_t)(sizeof(RAD_ISO_RING_COLORS) / sizeof(RAD_ISO_RING_COLORS[0]))))
    {
        return;
    }

    const float cx = (float)(screen_x + RAD_ISO_TILE_WIDTH / 2);
    const float cy = (float)(screen_y + RAD_ISO_TILE_HEIGHT / 2);
    const SDL_Color color = RAD_ISO_RING_COLORS[player];
    const float inner = 1.0f - RAD_ISO_RING_THICKNESS;

    SDL_Vertex vertices[RAD_ISO_RING_SEGMENTS * 6];
    for(int32_t i = 0; i < RAD_ISO_RING_SEGMENTS; ++i)
    {
        const float a0 = 2.0f * (float)M_PI * (float)i / RAD_ISO_RING_SEGMENTS;
        const float a1 = 2.0f * (float)M_PI * (float)(i + 1) / RAD_ISO_RING_SEGMENTS;
        const SDL_Vertex in0 = RAD_IsoRingVertex(cx, cy, a0, inner, color);
        const SDL_Vertex in1 = RAD_IsoRingVertex(cx, cy, a1, inner, color);
        const SDL_Vertex out0 = RAD_IsoRingVertex(cx, cy, a0, 1.0f, color);
        const SDL_Vertex out1 = RAD_IsoRingVertex(cx, cy, a1, 1.0f, color);
        SDL_Vertex *v = &vertices[i * 6];

        v[0] = in0;
        v[1] = out0;
        v[2] = out1;

        v[3] = in0;
        v[4] = out1;
        v[5] = in1;
    }

    // Wie beim Schatten: den Mischmodus des Aufrufers danach wiederherstellen.
    SDL_BlendMode previous = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &previous);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderGeometry(renderer, NULL, vertices, RAD_ISO_RING_SEGMENTS * 6, NULL, 0);
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
        // Nach dem Schatten, sonst dunkelt er den Ring ab.
        RAD_RenderIsoEntityRing(renderer, entity->player, screen_x, screen_y);
        SDL_RenderCopy(renderer, texture, &source, &target);
        return;
    }

    RAD_RenderIsoEntityRing(renderer, entity->player, screen_x, screen_y);
    SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
    SDL_Rect rect = {.x=screen_x,.y=screen_y-RAD_ISO_TILE_HEIGHT/2,.w=RAD_ISO_TILE_WIDTH,.h=RAD_ISO_TILE_HEIGHT};
    SDL_RenderFillRect(renderer, &rect);
}
