#include <radish/rendering/iso_facing.h>
#include <math.h>

RAD_IsoFacing_t RAD_IsoFacingOf(float dx, float dy, RAD_IsoFacing_t current)
{
    if((dx == 0.0f) && (dy == 0.0f))
    {
        return current;
    }
    if(fabsf(dx) >= fabsf(dy))
    {
        return (dx > 0.0f) ? RAD_ISO_FACING_X_PLUS : RAD_ISO_FACING_X_MINUS;
    }
    return (dy > 0.0f) ? RAD_ISO_FACING_Y_PLUS : RAD_ISO_FACING_Y_MINUS;
}

int32_t RAD_IsoFacingQuarterTurns(RAD_IsoFacing_t from, RAD_IsoFacing_t to)
{
    if(from < 0 || from >= RAD_ISO_FACINGS || to < 0 || to >= RAD_ISO_FACINGS)
    {
        return 0;
    }

    const int32_t turns = ((int32_t)to - (int32_t)from + RAD_ISO_FACINGS) % RAD_ISO_FACINGS;
    return (turns == 3) ? -1 : turns;
}

float RAD_IsoFacingAngle(RAD_IsoFacing_t facing)
{
    if(facing < 0 || facing >= RAD_ISO_FACINGS)
    {
        return 0.0f;
    }
    return 90.0f * (float)facing;
}

float RAD_IsoFacingAngleOf(float dx, float dy)
{
    if((dx == 0.0f) && (dy == 0.0f))
    {
        return 0.0f;
    }
    // X_MINUS (-1, 0) ist 0 Grad, Y_PLUS (0, 1) 90.
    return RAD_IsoAngleNormalize(atan2f(dy, -dx) * 180.0f / (float)M_PI);
}

float RAD_IsoAngleNormalize(float angle)
{
    float normalized = fmodf(angle, 360.0f);
    if(normalized < 0.0f)
    {
        normalized += 360.0f;
    }
    // Ein winzig negativer Rest rundet beim Addieren auf 360 auf.
    return (normalized >= 360.0f) ? 0.0f : normalized;
}

float RAD_IsoAngleDelta(float from, float to)
{
    const float delta = RAD_IsoAngleNormalize(to - from);
    return (delta > 180.0f) ? delta - 360.0f : delta;
}
