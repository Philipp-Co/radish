#include <radish/view/labeled_bar_view.h>
#include <radish/view/bar_view.h>
#include <radish/view/text_view.h>
#include <stdlib.h>


///
/// Anteil der TextView an der Breite, in Prozent, und Abstand zwischen TextView
/// und BarView, in Pixeln (view/labeled_bar_view.h).
///
#define RAD_LABELED_BAR_VIEW_LABEL_WIDTH_PERCENT 40
#define RAD_LABELED_BAR_VIEW_GAP 4


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_LabeledBarView
{
    SDL_Rect area;
    RAD_TextView_t *text_view;
    RAD_BarView_t *bar_view;
};


RAD_LabeledBarView_t* RAD_CreateLabeledBarView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font)
{
    RAD_LabeledBarView_t *labeled_bar_view = malloc(sizeof(struct RAD_LabeledBarView));
    if(labeled_bar_view == NULL)
    {
        return NULL;
    }

    const int32_t label_width = (width * RAD_LABELED_BAR_VIEW_LABEL_WIDTH_PERCENT) / 100;
    int32_t bar_width = width - label_width - RAD_LABELED_BAR_VIEW_GAP;
    if(bar_width < 0)
    {
        bar_width = 0;
    }

    *labeled_bar_view = (struct RAD_LabeledBarView){
        .area = { .x = x, .y = y, .w = width, .h = height },
        .text_view = RAD_CreateTextView(x, y, label_width, height, font),
        .bar_view = RAD_CreateBarView(x + width - bar_width, y, bar_width, height)
    };

    if(labeled_bar_view->text_view == NULL || labeled_bar_view->bar_view == NULL)
    {
        RAD_DestroyLabeledBarView(&labeled_bar_view);
        return NULL;
    }

    return labeled_bar_view;
}

///
/// Baut auch eine halb angelegte LabeledBarView ab (RAD_CreateLabeledBarView im
/// Fehlerfall): TextView oder BarView sind dann womoeglich NULL.
///
void RAD_DestroyLabeledBarView(RAD_LabeledBarView_t **labeled_bar_view)
{
    RAD_LabeledBarView_t *v = *labeled_bar_view;

    if(v->text_view != NULL)
    {
        RAD_DestroyTextView(&v->text_view);
    }
    if(v->bar_view != NULL)
    {
        RAD_DestroyBarView(&v->bar_view);
    }

    free(v);
    *labeled_bar_view = NULL;
}

bool RAD_LabeledBarViewSetLabel(RAD_LabeledBarView_t *labeled_bar_view, const char *label)
{
    return RAD_TextViewSetText(labeled_bar_view->text_view, label);
}

void RAD_LabeledBarViewSetValue(RAD_LabeledBarView_t *labeled_bar_view, int32_t current, int32_t maximum)
{
    RAD_BarViewSetValue(labeled_bar_view->bar_view, current, maximum);
}

void RAD_LabeledBarViewSetColors(RAD_LabeledBarView_t *labeled_bar_view, SDL_Color filled, SDL_Color remaining)
{
    RAD_BarViewSetColors(labeled_bar_view->bar_view, filled, remaining);
}

void RAD_UpdateLabeledBarView(RAD_LabeledBarView_t *labeled_bar_view, SDL_Renderer *renderer)
{
    RAD_UpdateTextView(labeled_bar_view->text_view, renderer);
    RAD_UpdateBarView(labeled_bar_view->bar_view, renderer);
}
