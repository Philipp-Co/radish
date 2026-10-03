#include <radish/view/bar_view.h>
#include <stdlib.h>


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_BarView
{
    SDL_Rect area;
    int32_t current;
    int32_t maximum;
    SDL_Color filled;
    SDL_Color remaining;
};


RAD_BarView_t* RAD_CreateBarView(int32_t x, int32_t y, int32_t width, int32_t height)
{
    RAD_BarView_t *bar_view = malloc(sizeof(struct RAD_BarView));
    if(bar_view == NULL)
    {
        return NULL;
    }

    *bar_view = (struct RAD_BarView){
        .area = { .x = x, .y = y, .w = width, .h = height },
        .current = 0,
        .maximum = 0,
        .filled = { 0, 192, 0, 255 },
        .remaining = { 192, 0, 0, 255 }
    };

    return bar_view;
}

void RAD_DestroyBarView(RAD_BarView_t **bar_view)
{
    free(*bar_view);
    *bar_view = NULL;
}

void RAD_BarViewSetValue(RAD_BarView_t *bar_view, int32_t current, int32_t maximum)
{
    bar_view->maximum = (maximum > 0) ? maximum : 0;

    if(current < 0)
    {
        current = 0;
    }
    if(current > bar_view->maximum)
    {
        current = bar_view->maximum;
    }
    bar_view->current = current;
}

void RAD_BarViewSetColors(RAD_BarView_t *bar_view, SDL_Color filled, SDL_Color remaining)
{
    bar_view->filled = filled;
    bar_view->remaining = remaining;
}

void RAD_UpdateBarView(RAD_BarView_t *bar_view, SDL_Renderer *renderer)
{
    // In 64 Bit gerechnet: Breite mal current passt nicht immer in 32.
    const int filled_width = (bar_view->maximum > 0)
        ? (int)(((int64_t)bar_view->area.w * bar_view->current) / bar_view->maximum)
        : 0;

    const SDL_Rect filled = {
        .x = bar_view->area.x, .y = bar_view->area.y,
        .w = filled_width, .h = bar_view->area.h
    };
    const SDL_Rect remaining = {
        .x = bar_view->area.x + filled_width, .y = bar_view->area.y,
        .w = bar_view->area.w - filled_width, .h = bar_view->area.h
    };

    if(filled.w > 0)
    {
        SDL_SetRenderDrawColor(renderer, bar_view->filled.r, bar_view->filled.g, bar_view->filled.b, bar_view->filled.a);
        SDL_RenderFillRect(renderer, &filled);
    }
    if(remaining.w > 0)
    {
        SDL_SetRenderDrawColor(renderer, bar_view->remaining.r, bar_view->remaining.g, bar_view->remaining.b, bar_view->remaining.a);
        SDL_RenderFillRect(renderer, &remaining);
    }
}
