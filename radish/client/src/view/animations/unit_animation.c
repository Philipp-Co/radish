#include <radish/view/animations/unit_animation.h>
#include <math.h>
#include <stdlib.h>


///
/// Legt einen leeren Plan mit Platz fuer "capacity" Abschnitte an; NULL, wenn
/// kein Speicher da ist.
///
static RAD_UnitAnimation_t* RAD_UnitAnimationAllocate(size_t capacity)
{
    RAD_UnitAnimation_t *unit_animation = malloc(sizeof(RAD_UnitAnimation_t));
    if(unit_animation == NULL)
    {
        return NULL;
    }

    unit_animation->phase_count = 0;
    unit_animation->phase_capacity = capacity;
    unit_animation->phases = calloc(capacity, sizeof(RAD_UnitAnimationPhase_t));
    if(unit_animation->phases == NULL)
    {
        free(unit_animation);
        return NULL;
    }
    return unit_animation;
}

///
/// Sorgt fuer Platz fuer "more" weitere Abschnitte. false, wenn kein Speicher da
/// ist; dann bleibt der Plan, wie er war.
///
static bool RAD_UnitAnimationReserve(RAD_UnitAnimation_t *unit_animation, size_t more)
{
    const size_t needed = unit_animation->phase_count + more;
    if(needed <= unit_animation->phase_capacity)
    {
        return true;
    }

    RAD_UnitAnimationPhase_t *phases = realloc(unit_animation->phases, needed * sizeof(RAD_UnitAnimationPhase_t));
    if(phases == NULL)
    {
        return false;
    }
    unit_animation->phases = phases;
    unit_animation->phase_capacity = needed;
    return true;
}

///
/// Haengt einen Abschnitt an; Platz muss reserviert sein. Gibt zurueck, wann er
/// endet.
///
static float RAD_UnitAnimationPush(RAD_UnitAnimation_t *unit_animation, RAD_UnitAnimationPhase_t phase)
{
    unit_animation->phases[unit_animation->phase_count++] = phase;
    return phase.start_ms + phase.duration_ms;
}

///
/// Haengt ab "at_ms" an, dass die Figur auf (x, y) in Richtung "facing" den Turm
/// von "from" auf "target" richtet, und, mit "back", feuert und nach "hold_ms"
/// nach vorn zurueck: ein, mit "back" drei Abschnitte, Platz muss reserviert sein.
/// Gibt zurueck, wann das endet.
///
static float RAD_UnitAnimationPushAim(RAD_UnitAnimation_t *unit_animation, float at_ms, float x, float y,
                                      RAD_IsoFacing_t facing, float from, float target, bool back, float hold_ms,
                                      float aim_ms)
{
    const RAD_UnitAnimationPhase_t aim = {
        .type = RAD_UNIT_ANIMATION_AIM,
        .from_x = x, .from_y = y, .to_x = x, .to_y = y,
        .from_facing = facing, .to_facing = facing
    };

    const float start = RAD_IsoAngleNormalize(from);
    const float to = RAD_IsoAngleNormalize(target);
    const float there = RAD_IsoAngleDelta(start, to);

    RAD_UnitAnimationPhase_t phase = aim;
    phase.start_ms = at_ms;
    phase.duration_ms = aim_ms * fabsf(there) / 90.0f;
    phase.from_aim = start;
    phase.to_aim = start + there;
    at_ms = RAD_UnitAnimationPush(unit_animation, phase);

    if(back)
    {
        phase = aim;
        phase.type = RAD_UNIT_ANIMATION_FIRE;
        phase.start_ms = at_ms;
        phase.duration_ms = hold_ms;
        phase.from_aim = to;
        phase.to_aim = to;
        at_ms = RAD_UnitAnimationPush(unit_animation, phase);

        const float home = RAD_IsoAngleDelta(to, 0.0f);
        phase = aim;
        phase.start_ms = at_ms;
        phase.duration_ms = aim_ms * fabsf(home) / 90.0f;
        phase.from_aim = to;
        phase.to_aim = to + home;
        at_ms = RAD_UnitAnimationPush(unit_animation, phase);
    }
    return at_ms;
}

RAD_UnitAnimation_t* RAD_CreateUnitAnimation(const float *xs, const float *ys, size_t count,
                                             RAD_IsoFacing_t facing, float aim_angle,
                                             float start_ms, float step_ms, float turn_ms, float aim_ms)
{
    if(count == 0)
    {
        return NULL;
    }

    // Je Abschnitt des Pfades hoechstens eine Drehung und eine Fahrt, davor
    // womoeglich der Turm zurueck.
    const size_t segment_count = (count > 1) ? (count - 1) : 1;
    RAD_UnitAnimation_t *unit_animation = RAD_UnitAnimationAllocate(2 * segment_count + 1);
    if(unit_animation == NULL)
    {
        return NULL;
    }

    float at_ms = start_ms;
    if(RAD_IsoAngleNormalize(aim_angle) != 0.0f)
    {
        at_ms = RAD_UnitAnimationPushAim(unit_animation, at_ms, xs[0], ys[0], facing, aim_angle, 0.0f,
                                         false, 0.0f, aim_ms);
    }

    for(size_t i = 0; i < segment_count; ++i)
    {
        const size_t next = (i + 1 < count) ? (i + 1) : i;
        const RAD_IsoFacing_t heading = RAD_IsoFacingOf(xs[next] - xs[i], ys[next] - ys[i], facing);

        const int32_t turns = abs(RAD_IsoFacingQuarterTurns(facing, heading));
        if(turns > 0)
        {
            at_ms = RAD_UnitAnimationPush(unit_animation, (RAD_UnitAnimationPhase_t){
                .type = RAD_UNIT_ANIMATION_TURN,
                .start_ms = at_ms,
                .duration_ms = turn_ms * (float)turns,
                .from_x = xs[i], .from_y = ys[i],
                .to_x = xs[i], .to_y = ys[i],
                .from_facing = facing,
                .to_facing = heading
            });
            facing = heading;
        }

        at_ms = RAD_UnitAnimationPush(unit_animation, (RAD_UnitAnimationPhase_t){
            .type = RAD_UNIT_ANIMATION_DRIVE,
            .start_ms = at_ms,
            .duration_ms = step_ms,
            .from_x = xs[i], .from_y = ys[i],
            .to_x = xs[next], .to_y = ys[next],
            .from_facing = facing,
            .to_facing = facing
        });
    }

    return unit_animation;
}

RAD_UnitAnimation_t* RAD_CreateUnitAimAnimation(float x, float y, RAD_IsoFacing_t facing, float aim_angle,
                                                float target_angle, bool back, float hold_ms,
                                                float start_ms, float aim_ms)
{
    RAD_UnitAnimation_t *unit_animation = RAD_UnitAnimationAllocate(3);
    if(unit_animation == NULL)
    {
        return NULL;
    }

    RAD_UnitAnimationPushAim(unit_animation, start_ms, x, y, facing, aim_angle, target_angle,
                             back, hold_ms, aim_ms);
    return unit_animation;
}

void RAD_DestroyUnitAnimation(RAD_UnitAnimation_t **unit_animation)
{
    free((*unit_animation)->phases);
    free(*unit_animation);
    *unit_animation = NULL;
}

bool RAD_UnitAnimationAppendAim(RAD_UnitAnimation_t *unit_animation, float target_angle, bool back, float hold_ms,
                                float aim_ms)
{
    if(!RAD_UnitAnimationReserve(unit_animation, 3))
    {
        return false;
    }

    const RAD_UnitAnimationPhase_t *last = &unit_animation->phases[unit_animation->phase_count - 1];
    RAD_UnitAnimationPushAim(unit_animation, last->start_ms + last->duration_ms, last->to_x, last->to_y,
                             last->to_facing, last->to_aim, target_angle, back, hold_ms, aim_ms);
    return true;
}

RAD_UnitAnimationState_t RAD_UnitAnimationStateAt(const RAD_UnitAnimation_t *unit_animation, float now_ms)
{
    // Der Abschnitt, der zu "now_ms" laeuft; nach dem Ende der letzte.
    size_t index = unit_animation->phase_count - 1;
    for(size_t i = 0; i < unit_animation->phase_count; ++i)
    {
        const RAD_UnitAnimationPhase_t *phase = &unit_animation->phases[i];
        if(now_ms < phase->start_ms + phase->duration_ms)
        {
            index = i;
            break;
        }
    }

    const RAD_UnitAnimationPhase_t *phase = &unit_animation->phases[index];
    float progress = 1.0f;
    if(phase->duration_ms > 0.0f)
    {
        progress = (now_ms - phase->start_ms) / phase->duration_ms;
        progress = (progress < 0.0f) ? 0.0f : ((progress > 1.0f) ? 1.0f : progress);
    }

    RAD_UnitAnimationState_t state = {
        .x = phase->from_x + (phase->to_x - phase->from_x) * progress,
        .y = phase->from_y + (phase->to_y - phase->from_y) * progress,
        .facing = phase->from_facing,
        .turning = false,
        .turn_to = phase->to_facing,
        .turn_progress = 0.0f,
        .aim_angle = RAD_IsoAngleNormalize(phase->from_aim + (phase->to_aim - phase->from_aim) * progress)
    };

    if(phase->type == RAD_UNIT_ANIMATION_TURN)
    {
        if(progress < 1.0f)
        {
            state.turning = true;
            state.turn_progress = progress;
        }
        else
        {
            state.facing = phase->to_facing;
        }
    }
    return state;
}

bool RAD_UnitAnimationFireMs(const RAD_UnitAnimation_t *unit_animation, float *fire_ms)
{
    for(size_t i = unit_animation->phase_count; i > 0; --i)
    {
        const RAD_UnitAnimationPhase_t *phase = &unit_animation->phases[i - 1];
        if(phase->type == RAD_UNIT_ANIMATION_FIRE)
        {
            *fire_ms = phase->start_ms;
            return true;
        }
    }
    return false;
}

bool RAD_UnitAnimationIsFinished(const RAD_UnitAnimation_t *unit_animation, float now_ms)
{
    const RAD_UnitAnimationPhase_t *last = &unit_animation->phases[unit_animation->phase_count - 1];
    return now_ms >= last->start_ms + last->duration_ms;
}
