#include <radish/view/animations/move_animation.h>
#include <stdlib.h>


RAD_MoveAnimation_t* RAD_CreateMoveAnimation(const float *xs, const float *ys, size_t count,
                                             float start_ms, float step_duration_ms,
                                             float *x_out, float *y_out)
{
    if(count == 0)
    {
        return NULL;
    }

    RAD_MoveAnimation_t *move_animation = malloc(sizeof(RAD_MoveAnimation_t));
    if(move_animation == NULL)
    {
        return NULL;
    }

    move_animation->segment_count = (count > 1) ? (count - 1) : 1;
    move_animation->start_ms = start_ms;
    move_animation->step_duration_ms = step_duration_ms;
    move_animation->x_interpolations = calloc(move_animation->segment_count, sizeof(RAD_Interpolation_t*));
    move_animation->y_interpolations = calloc(move_animation->segment_count, sizeof(RAD_Interpolation_t*));
    if(move_animation->x_interpolations == NULL || move_animation->y_interpolations == NULL)
    {
        RAD_DestroyMoveAnimation(&move_animation);
        return NULL;
    }

    for(size_t i = 0; i < move_animation->segment_count; ++i)
    {
        const size_t next = (i + 1 < count) ? (i + 1) : i;
        const float from_ms = start_ms + step_duration_ms * (float)i;
        const float to_ms = from_ms + step_duration_ms;

        move_animation->x_interpolations[i] = RAD_CreateInterpolation(
            (RAD_InterpolationPoint_t){ .value = xs[i], .timestamp_ms = from_ms },
            (RAD_InterpolationPoint_t){ .value = xs[next], .timestamp_ms = to_ms },
            x_out);
        move_animation->y_interpolations[i] = RAD_CreateInterpolation(
            (RAD_InterpolationPoint_t){ .value = ys[i], .timestamp_ms = from_ms },
            (RAD_InterpolationPoint_t){ .value = ys[next], .timestamp_ms = to_ms },
            y_out);

        if(move_animation->x_interpolations[i] == NULL || move_animation->y_interpolations[i] == NULL)
        {
            RAD_DestroyMoveAnimation(&move_animation);
            return NULL;
        }
    }

    return move_animation;
}

///
/// Baut auch eine halb angelegte MoveAnimation ab (RAD_CreateMoveAnimation im
/// Fehlerfall): Arrays und einzelne Interpolationen sind dann womoeglich NULL.
///
void RAD_DestroyMoveAnimation(RAD_MoveAnimation_t **move_animation)
{
    RAD_MoveAnimation_t *m = *move_animation;

    for(size_t i = 0; i < m->segment_count; ++i)
    {
        if(m->x_interpolations != NULL && m->x_interpolations[i] != NULL)
        {
            RAD_DestroyInterpolation(&m->x_interpolations[i]);
        }
        if(m->y_interpolations != NULL && m->y_interpolations[i] != NULL)
        {
            RAD_DestroyInterpolation(&m->y_interpolations[i]);
        }
    }

    free(m->x_interpolations);
    free(m->y_interpolations);
    free(m);
    *move_animation = NULL;
}

void RAD_UpdateMoveAnimation(RAD_MoveAnimation_t *move_animation, float now_ms)
{
    const size_t last = move_animation->segment_count - 1;
    const float elapsed_ms = now_ms - move_animation->start_ms;

    size_t segment = last;
    if(elapsed_ms <= 0.0f)
    {
        segment = 0;
    }
    else if(move_animation->step_duration_ms > 0.0f)
    {
        const float steps = elapsed_ms / move_animation->step_duration_ms;
        if(steps < (float)last)
        {
            segment = (size_t)steps;
        }
    }

    RAD_UpdateInterpolation(move_animation->x_interpolations[segment], now_ms);
    RAD_UpdateInterpolation(move_animation->y_interpolations[segment], now_ms);
}

bool RAD_MoveAnimationIsFinished(const RAD_MoveAnimation_t *move_animation, float now_ms)
{
    const float duration_ms = move_animation->step_duration_ms * (float)move_animation->segment_count;
    return now_ms >= move_animation->start_ms + duration_ms;
}
