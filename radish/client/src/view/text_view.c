#include <radish/view/text_view.h>
#include <stdlib.h>
#include <string.h>


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_TextView
{
    SDL_Rect area;
    const RAD_Font_t *font;

    ///
    /// Eigene Kopie des Textes, NULL heisst leer.
    ///
    char *text;
    SDL_Color color;
};


RAD_TextView_t* RAD_CreateTextView(int32_t x, int32_t y, int32_t width, int32_t height, const RAD_Font_t *font)
{
    RAD_TextView_t *text_view = malloc(sizeof(struct RAD_TextView));
    if(text_view == NULL)
    {
        return NULL;
    }

    *text_view = (struct RAD_TextView){
        .area = { .x = x, .y = y, .w = width, .h = height },
        .font = font,
        .text = NULL,
        .color = { 255, 255, 255, 255 }
    };

    return text_view;
}

void RAD_DestroyTextView(RAD_TextView_t **text_view)
{
    RAD_TextView_t *v = *text_view;

    free(v->text);
    free(v);
    *text_view = NULL;
}

bool RAD_TextViewSetText(RAD_TextView_t *text_view, const char *text)
{
    char *copy = NULL;
    if(text != NULL && text[0] != '\0')
    {
        const size_t size = strlen(text) + 1;
        copy = malloc(size);
        if(copy == NULL)
        {
            return false;
        }
        memcpy(copy, text, size);
    }

    free(text_view->text);
    text_view->text = copy;
    return true;
}

void RAD_TextViewSetPosition(RAD_TextView_t *text_view, int32_t x, int32_t y)
{
    text_view->area.x = x;
    text_view->area.y = y;
}

void RAD_TextViewSetColor(RAD_TextView_t *text_view, SDL_Color color)
{
    text_view->color = color;
}

int32_t RAD_TextViewTextWidth(const RAD_TextView_t *text_view)
{
    if(text_view->text == NULL)
    {
        return 0;
    }
    return RAD_FontMeasure(text_view->font, text_view->text, strlen(text_view->text));
}

void RAD_UpdateTextView(RAD_TextView_t *text_view, SDL_Renderer *renderer)
{
    if(text_view->text == NULL)
    {
        return;
    }

    // Was nicht in die Flaeche passt, schneidet die Schrift ab.
    RAD_FontDraw(text_view->font, renderer, text_view->text, strlen(text_view->text),
                 text_view->area.x, text_view->area.y, text_view->color, &text_view->area);
}
