#include <radish/view/animations/interpolation.h>
#include <stdlib.h>


RAD_Interpolation_t* RAD_CreateInterpolation(RAD_InterpolationPoint_t from, RAD_InterpolationPoint_t to,
                                             float *result)
{
    RAD_Interpolation_t *interpolation = malloc(sizeof(RAD_Interpolation_t));
    if(interpolation == NULL)
    {
        return NULL;
    }

    interpolation->from = from;
    interpolation->to = to;
    interpolation->result = result;

    const float duration_ms = to.timestamp_ms - from.timestamp_ms;
    interpolation->speed = (duration_ms > 0.0f) ? ((to.value - from.value) / duration_ms) : 0.0f;

    return interpolation;
}

void RAD_DestroyInterpolation(RAD_Interpolation_t **interpolation)
{
    free(*interpolation);
    *interpolation = NULL;
}

float RAD_InterpolationCurrentValue(const RAD_Interpolation_t *interpolation, float now_ms)
{
    if(now_ms >= interpolation->to.timestamp_ms)
    {
        return interpolation->to.value;
    }

    if(now_ms <= interpolation->from.timestamp_ms)
    {
        return interpolation->from.value;
    }

    const float elapsed_ms = now_ms - interpolation->from.timestamp_ms;
    return interpolation->from.value + interpolation->speed * elapsed_ms;
}

void RAD_UpdateInterpolation(RAD_Interpolation_t *interpolation, float now_ms)
{
    *interpolation->result = RAD_InterpolationCurrentValue(interpolation, now_ms);
}
