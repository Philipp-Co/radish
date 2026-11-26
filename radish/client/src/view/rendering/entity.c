#include <radish/view/rendering/entity.h>
#include <radish/view/rendering/iso_definitions.h>
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

    // Eine Folge, wenn eine laeuft und ihr Bild zur Lage passt (view/rendering/entity.h).
    RAD_IsoUnitAtlas_t sequence_atlas = RAD_ISO_UNIT_ATLAS_TURN;
    const bool sequence_fits = entity->sequence_active
                               && ((entity->sequence != RAD_ISO_UNIT_ANIMATION_FIRE) || (entity->aim_angle == 0.0f));
    const int32_t sequence_frame = sequence_fits
                                   ? RAD_IsoUnitSequenceFrameOf(entity->sprite, entity->sequence, entity->facing,
                                                                entity->sequence_ms, &sequence_atlas)
                                   : -1;

    if(entity->turning)
    {
        frame = RAD_IsoUnitTurnFrame(entity->sprite, entity->facing, entity->turn_to, entity->turn_progress);
    }
    else if(sequence_frame >= 0)
    {
        atlas = sequence_atlas;
        frame = sequence_frame;
    }
    else if((entity->aim_angle != 0.0f) && RAD_IsoUnitCanAim(entity->sprite))
    {
        atlas = RAD_IsoUnitAimAtlas(entity->facing);
        frame = RAD_IsoUnitAimFrame(entity->sprite, entity->facing, entity->aim_angle);
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


///
/// Was die Figur gerade abspielt (view/rendering/entity.h). Die Animation sagt, wo
/// sie dabei steht und wohin sie schaut; hier steht nur, was daraus wird.
///

///
/// Beendet, was gerade laeuft, und gibt den Plan frei. Was davon zu sehen ist --
/// Blickrichtung, Turmwinkel, Position -- bleibt stehen.
///
static void RAD_IsoEntityStop(RAD_IsoEntity_t *entity)
{
    if(entity->animation != NULL)
    {
        RAD_DestroyUnitAnimation(&entity->animation);
    }
    entity->activity = RAD_ISO_ENTITY_IDLE;
}

///
/// Wie weit der Turm einer Figur auf (x, y) mit Blick "facing" gegen die Wanne
/// gedreht sein muss, damit sein Rohr auf die Mitte von Feld (target_x,
/// target_y) zeigt.
///
static float RAD_IsoEntityAimAngleTo(int32_t x, int32_t y, RAD_IsoFacing_t facing,
                                     int32_t target_x, int32_t target_y)
{
    const float toward = RAD_IsoFacingAngleOf((float)(target_x - x), (float)(target_y - y));
    return RAD_IsoAngleNormalize(toward - RAD_IsoFacingAngle(facing));
}

///
/// Uebertraegt, was die Figur laut Animation gerade tut, auf die Folge ihres
/// Bildes (RAD_IsoEntity_t.sequence): waehrend einer Fahrt MOVE, gezaehlt ab
/// "move_ms" seit Beginn des Zuges, waehrend eines Schusses FIRE, gezaehlt ab
/// dessen Beginn; sonst keine.
///
static void RAD_IsoEntityApplySequence(RAD_IsoEntity_t *entity, const RAD_UnitAnimationState_t *state,
                                       float move_ms)
{
    entity->sequence_active = state->driving || state->firing;
    if(state->firing)
    {
        entity->sequence = RAD_ISO_UNIT_ANIMATION_FIRE;
        entity->sequence_ms = state->firing_ms;
    }
    else if(state->driving)
    {
        entity->sequence = RAD_ISO_UNIT_ANIMATION_MOVE;
        entity->sequence_ms = move_ms;
    }
}

bool RAD_IsoEntityMoveAlong(RAD_IsoEntity_t *entity, const float *xs, const float *ys, size_t count,
                            int32_t to_x, int32_t to_y, float now_ms)
{
    // Mitten in einer Drehung: sie zaehlt ab der Haelfte als vollzogen, damit
    // der neue Zug von der Richtung ausgeht, die zu sehen ist.
    if(entity->turning)
    {
        if(entity->turn_progress >= 0.5f)
        {
            entity->facing = entity->turn_to;
        }
        entity->turning = false;
    }

    // Noch nie ausgerichtet: in die Richtung des ersten Schrittes.
    const RAD_IsoFacing_t facing = entity->facing_set
                                   ? entity->facing
                                   : RAD_IsoFacingOf((count > 1) ? xs[1] - xs[0] : 0.0f,
                                                     (count > 1) ? ys[1] - ys[0] : 0.0f, entity->facing);

    RAD_IsoEntityStop(entity);
    entity->activity = RAD_ISO_ENTITY_MOVING;
    entity->start_ms = now_ms;
    entity->to_x = to_x;
    entity->to_y = to_y;
    entity->animation = RAD_CreateUnitAnimation(xs, ys, count, facing, entity->aim_angle, now_ms,
                                                RAD_IsoUnitStepMs(entity->sprite),
                                                RAD_IsoUnitTurnMs(entity->sprite),
                                                RAD_IsoUnitAimMs(entity->sprite));
    if(entity->animation == NULL)
    {
        // Ohne Animation gleich ans Ziel -- wie ein Zug, der schon abgelaufen ist.
        entity->activity = RAD_ISO_ENTITY_IDLE;
        entity->x = (float)to_x;
        entity->y = (float)to_y;
        return false;
    }
    return true;
}

bool RAD_IsoEntityAim(RAD_IsoEntity_t *entity, bool has_target, int32_t target_x, int32_t target_y,
                      bool back, float hold_ms, float now_ms, float *fire_ms)
{
    if(!RAD_IsoUnitCanAim(entity->sprite))
    {
        return false;
    }

    // Auf das eigene Feld zielt niemand -- auch nicht auf das, wo der Zug endet.
    int32_t x = 0, y = 0;
    RAD_IsoEntityRestingPlace(entity, &x, &y);
    if(has_target && (target_x == x) && (target_y == y))
    {
        return false;
    }

    // Waehrend eines Zuges haengt das Zielen hinten an: gerechnet von dort, wo
    // er endet, und von der Richtung, in die sie dann schaut.
    if(entity->activity == RAD_ISO_ENTITY_MOVING)
    {
        const RAD_UnitAnimationState_t end = RAD_UnitAnimationStateAt(entity->animation, INFINITY);
        const float target_angle = has_target ? RAD_IsoEntityAimAngleTo(x, y, end.facing, target_x, target_y) : 0.0f;
        if(!RAD_UnitAnimationAppendAim(entity->animation, target_angle, back, hold_ms,
                                       RAD_IsoUnitAimMs(entity->sprite)))
        {
            return false;
        }
        return back && RAD_UnitAnimationFireMs(entity->animation, fire_ms);
    }

    // Ein Wrack zielt nicht mehr; ein laufendes Zielen wird ersetzt.
    if(entity->activity == RAD_ISO_ENTITY_WRECK)
    {
        return false;
    }

    const float target_angle = has_target ? RAD_IsoEntityAimAngleTo(x, y, entity->facing, target_x, target_y) : 0.0f;
    RAD_IsoEntityStop(entity);
    entity->animation = RAD_CreateUnitAimAnimation((float)x, (float)y, entity->facing, entity->aim_angle,
                                                   target_angle, back, hold_ms, now_ms,
                                                   RAD_IsoUnitAimMs(entity->sprite));
    if(entity->animation == NULL)
    {
        return false;
    }

    entity->activity = RAD_ISO_ENTITY_AIMING;
    entity->start_ms = now_ms;
    return back && RAD_UnitAnimationFireMs(entity->animation, fire_ms);
}

void RAD_IsoEntityDestroy(RAD_IsoEntity_t *entity, float impact_ms, float end_ms)
{
    RAD_IsoEntityStop(entity);
    entity->activity = RAD_ISO_ENTITY_WRECK;
    entity->start_ms = impact_ms;
    entity->end_ms = end_ms;
    entity->turning = false;
    entity->sequence_active = false;
}

RAD_IsoEntityActivity_t RAD_IsoEntityUpdate(RAD_IsoEntity_t *entity, float now_ms)
{
    switch(entity->activity)
    {
        case RAD_ISO_ENTITY_MOVING:
        {
            const RAD_UnitAnimationState_t state = RAD_UnitAnimationStateAt(entity->animation, now_ms);
            entity->x = state.x;
            entity->y = state.y;
            entity->facing = state.facing;
            entity->facing_set = true;
            entity->turning = state.turning;
            entity->turn_to = state.turn_to;
            entity->turn_progress = state.turn_progress;
            entity->aim_angle = state.aim_angle;
            RAD_IsoEntityApplySequence(entity, &state, now_ms - entity->start_ms);

            if(!RAD_UnitAnimationIsFinished(entity->animation, now_ms))
            {
                return RAD_ISO_ENTITY_IDLE;
            }

            // Genau aufs Zielfeld, nicht auf das, was die Animation zuletzt
            // gerechnet hat: danach steht sie wieder auf ganzen Zahlen, und
            // RAD_IsoMapEntityAt findet sie dort (view/rendering/iso_map.h).
            entity->x = (float)entity->to_x;
            entity->y = (float)entity->to_y;
            entity->turning = false;
            entity->sequence_active = false;
            RAD_IsoEntityStop(entity);
            return RAD_ISO_ENTITY_MOVING;
        }

        case RAD_ISO_ENTITY_AIMING:
        {
            const RAD_UnitAnimationState_t state = RAD_UnitAnimationStateAt(entity->animation, now_ms);
            entity->aim_angle = state.aim_angle;

            // Die Figur zieht nicht: nur der Turm und der Schuss zaehlen, und
            // die Folge FIRE beginnt mit ihm (RAD_UnitAnimationFireMs).
            RAD_IsoEntityApplySequence(entity, &state, 0.0f);

            if(!RAD_UnitAnimationIsFinished(entity->animation, now_ms))
            {
                return RAD_ISO_ENTITY_IDLE;
            }

            entity->sequence_active = false;
            RAD_IsoEntityStop(entity);
            return RAD_ISO_ENTITY_AIMING;
        }

        case RAD_ISO_ENTITY_WRECK:
            if(now_ms >= entity->end_ms)
            {
                RAD_IsoEntityStop(entity);
                return RAD_ISO_ENTITY_WRECK;
            }

            // Vor dem Einschlag steht sie noch heil da.
            entity->sequence_active = (now_ms >= entity->start_ms);
            entity->sequence = RAD_ISO_UNIT_ANIMATION_DESTROYED;
            entity->sequence_ms = now_ms - entity->start_ms;
            return RAD_ISO_ENTITY_IDLE;

        case RAD_ISO_ENTITY_IDLE:
        default:
            return RAD_ISO_ENTITY_IDLE;
    }
}

bool RAD_IsoEntityHoldsPosition(const RAD_IsoEntity_t *entity)
{
    return (entity->activity == RAD_ISO_ENTITY_MOVING) || (entity->activity == RAD_ISO_ENTITY_WRECK);
}

bool RAD_IsoEntityIsBusy(const RAD_IsoEntity_t *entity)
{
    float fire_ms = 0.0f;
    switch(entity->activity)
    {
        case RAD_ISO_ENTITY_MOVING:
        case RAD_ISO_ENTITY_WRECK:
            return true;

        // Ein Zielen ohne Schuss ist die Vorschau aus dem Kontextmenue.
        case RAD_ISO_ENTITY_AIMING:
            return RAD_UnitAnimationFireMs(entity->animation, &fire_ms);

        case RAD_ISO_ENTITY_IDLE:
        default:
            return false;
    }
}

void RAD_IsoEntityRestingPlace(const RAD_IsoEntity_t *entity, int32_t *x, int32_t *y)
{
    if(entity->activity == RAD_ISO_ENTITY_MOVING)
    {
        *x = entity->to_x;
        *y = entity->to_y;
        return;
    }

    *x = (int32_t)entity->x;
    *y = (int32_t)entity->y;
}
