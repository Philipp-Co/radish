#include <radish/view/text_view.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_TextView
{
    SDL_Rect area;
    TTF_Font *font;

    ///
    /// Eigene Kopie des Textes, NULL heisst leer.
    ///
    char *text;
    SDL_Color color;

    ///
    /// Der gerenderte Text. Nach jedem Setter veraltet (texture_stale) und im
    /// naechsten RAD_UpdateTextView neu angelegt -- erst dort gibt es einen
    /// Renderer. NULL, wenn der Text leer ist oder SDL_ttf ihn nicht rendern
    /// konnte.
    ///
    bool texture_stale;
    SDL_Texture *texture;
};


static void RAD_TextViewBuildTexture(RAD_TextView_t *text_view, SDL_Renderer *renderer);


RAD_TextView_t* RAD_CreateTextView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font)
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
        .color = { 255, 255, 255, 255 },
        .texture_stale = false,
        .texture = NULL
    };

    return text_view;
}

void RAD_DestroyTextView(RAD_TextView_t **text_view)
{
    RAD_TextView_t *v = *text_view;

    if(v->texture != NULL)
    {
        SDL_DestroyTexture(v->texture);
    }
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
    text_view->texture_stale = true;
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
    text_view->texture_stale = true;
}

int32_t RAD_TextViewTextWidth(const RAD_TextView_t *text_view)
{
    int width = 0;
    if(text_view->text == NULL || TTF_SizeUTF8(text_view->font, text_view->text, &width, NULL) != 0)
    {
        return 0;
    }
    return width;
}

void RAD_UpdateTextView(RAD_TextView_t *text_view, SDL_Renderer *renderer)
{
    if(text_view->texture_stale)
    {
        RAD_TextViewBuildTexture(text_view, renderer);
        text_view->texture_stale = false;
    }

    if(text_view->texture == NULL)
    {
        return;
    }

    // Nur den Teil der Textur, der in die Flaeche passt.
    int w = 0, h = 0;
    SDL_QueryTexture(text_view->texture, NULL, NULL, &w, &h);
    if(w > text_view->area.w)
    {
        w = text_view->area.w;
    }
    if(h > text_view->area.h)
    {
        h = text_view->area.h;
    }

    const SDL_Rect source = { .x = 0, .y = 0, .w = w, .h = h };
    const SDL_Rect destination = { .x = text_view->area.x, .y = text_view->area.y, .w = w, .h = h };
    SDL_RenderCopy(renderer, text_view->texture, &source, &destination);
}


static void RAD_TextViewBuildTexture(RAD_TextView_t *text_view, SDL_Renderer *renderer)
{
    if(text_view->texture != NULL)
    {
        SDL_DestroyTexture(text_view->texture);
        text_view->texture = NULL;
    }

    // Einen leeren Text rendert TTF_RenderUTF8_Blended nicht.
    if(text_view->text == NULL)
    {
        return;
    }

    SDL_Surface *surface = TTF_RenderUTF8_Blended(text_view->font, text_view->text, text_view->color);
    if(surface == NULL)
    {
        printf("TTF_RenderUTF8_Blended \"%s\": %s\n", text_view->text, TTF_GetError());
        return;
    }

    text_view->texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if(text_view->texture == NULL)
    {
        printf("SDL_CreateTextureFromSurface: %s\n", SDL_GetError());
    }
}
