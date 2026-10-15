#include <radish/view/typewriter_view.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


///
/// So lange dauert es, bis das naechste Zeichen erscheint.
///
#define RAD_TYPEWRITER_MS_PER_CHARACTER 100

///
/// Ist der Text ganz da, ist der Cursor so lange an und dann so lange aus. Der
/// Takt haengt nur an der Uhr, nicht daran, wann der Text fertig war: so
/// blinken alle TypewriterViews gleichzeitig.
///
#define RAD_TYPEWRITER_CURSOR_BLINK_MS 500

///
/// Das Zeichen, das als Cursor gezeichnet wird.
///
#define RAD_TYPEWRITER_CURSOR "_"


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_TypewriterView
{
    SDL_Rect area;
    TTF_Font *font;

    ///
    /// Eigene Kopie des Textes, NULL heisst leer.
    ///
    char *text;
    SDL_Color color;

    ///
    /// Wie viele Zeichen (nicht Bytes) der Text hat.
    ///
    size_t character_count;

    ///
    /// Wann das Tippen begonnen hat, nach SDL_GetTicks64. Nach
    /// RAD_TypewriterViewSetText ist "started" false; das naechste
    /// RAD_UpdateTypewriterView setzt "start_ms" und beginnt von vorn.
    ///
    bool started;
    uint64_t start_ms;

    ///
    /// Wie viele Zeichen beim letzten Update sichtbar waren und wie breit sie
    /// in der Schrift sind. Neu gemessen nur, wenn sich die Zahl aendert
    /// oder die Texturen neu angelegt werden.
    ///
    size_t visible_count;
    int32_t visible_width;

    ///
    /// Der ganze Text und der Cursor, gerendert. Nach jedem Setter veraltet
    /// (textures_stale) und im naechsten RAD_UpdateTypewriterView neu angelegt
    /// -- erst dort gibt es einen Renderer. NULL, wenn es nichts zu rendern
    /// gibt oder SDL_ttf es nicht rendern konnte.
    ///
    bool textures_stale;
    SDL_Texture *text_texture;
    SDL_Texture *cursor_texture;
};


static void RAD_TypewriterViewBuildTextures(RAD_TypewriterView_t *typewriter_view, SDL_Renderer *renderer);
static void RAD_TypewriterViewDestroyTextures(RAD_TypewriterView_t *typewriter_view);
static SDL_Texture* RAD_TypewriterViewRender(TTF_Font *font, const char *text, SDL_Color color, SDL_Renderer *renderer);
static int32_t RAD_TypewriterViewMeasure(RAD_TypewriterView_t *typewriter_view, size_t count);
static size_t RAD_Utf8CharacterCount(const char *text);
static size_t RAD_Utf8ByteOffset(const char *text, size_t count);


RAD_TypewriterView_t* RAD_CreateTypewriterView(int32_t x, int32_t y, int32_t width, int32_t height)
{
    RAD_TypewriterView_t *typewriter_view = malloc(sizeof(struct RAD_TypewriterView));
    if(typewriter_view == NULL)
    {
        return NULL;
    }

    *typewriter_view = (struct RAD_TypewriterView){
        .area = { .x = x, .y = y, .w = width, .h = height },
        .font = NULL,
        .text = NULL,
        .color = { 255, 255, 255, 255 },
        .character_count = 0,
        .started = false,
        .start_ms = 0,
        .visible_count = 0,
        .visible_width = 0,
        .textures_stale = false,
        .text_texture = NULL,
        .cursor_texture = NULL
    };

    return typewriter_view;
}

void RAD_DestroyTypewriterView(RAD_TypewriterView_t **typewriter_view)
{
    RAD_TypewriterView_t *v = *typewriter_view;

    RAD_TypewriterViewDestroyTextures(v);
    free(v->text);
    free(v);
    *typewriter_view = NULL;
}

bool RAD_TypewriterViewSetText(RAD_TypewriterView_t *typewriter_view, const char *text)
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

    free(typewriter_view->text);
    typewriter_view->text = copy;
    typewriter_view->character_count = RAD_Utf8CharacterCount(copy);
    typewriter_view->started = false;
    typewriter_view->textures_stale = true;
    return true;
}

void RAD_TypewriterViewSetFont(RAD_TypewriterView_t *typewriter_view, TTF_Font *font)
{
    typewriter_view->font = font;
    typewriter_view->textures_stale = true;
}

void RAD_TypewriterViewSetColor(RAD_TypewriterView_t *typewriter_view, SDL_Color color)
{
    typewriter_view->color = color;
    typewriter_view->textures_stale = true;
}

int32_t RAD_TypewriterViewTextWidth(const RAD_TypewriterView_t *typewriter_view)
{
    if(typewriter_view->font == NULL || typewriter_view->text == NULL)
    {
        return 0;
    }

    int text_width = 0;
    int cursor_width = 0;
    if(TTF_SizeUTF8(typewriter_view->font, typewriter_view->text, &text_width, NULL) != 0
       || TTF_SizeUTF8(typewriter_view->font, RAD_TYPEWRITER_CURSOR, &cursor_width, NULL) != 0)
    {
        return 0;
    }
    return text_width + cursor_width;
}

void RAD_UpdateTypewriterView(RAD_TypewriterView_t *typewriter_view, SDL_Renderer *renderer)
{
    RAD_TypewriterView_t *v = typewriter_view;
    const uint64_t now = SDL_GetTicks64();

    if(!v->started)
    {
        v->started = true;
        v->start_ms = now;
    }

    if(v->textures_stale)
    {
        RAD_TypewriterViewBuildTextures(v, renderer);
        v->textures_stale = false;
        v->visible_count = 0;
        v->visible_width = 0;
    }

    if(v->text_texture == NULL)
    {
        return;
    }

    // Wie viele Zeichen sind schon getippt?
    const uint64_t elapsed = now - v->start_ms;
    size_t count = (size_t)(elapsed / RAD_TYPEWRITER_MS_PER_CHARACTER);
    if(count > v->character_count)
    {
        count = v->character_count;
    }
    if(count != v->visible_count)
    {
        v->visible_count = count;
        v->visible_width = RAD_TypewriterViewMeasure(v, count);
    }

    // Vom ganzen Text nur den getippten Anfang, und nur was in die Flaeche passt.
    int text_h = 0;
    SDL_QueryTexture(v->text_texture, NULL, NULL, NULL, &text_h);
    const int w = v->visible_width < v->area.w ? v->visible_width : v->area.w;
    const int h = text_h < v->area.h ? text_h : v->area.h;
    if(w > 0 && h > 0)
    {
        const SDL_Rect source = { .x = 0, .y = 0, .w = w, .h = h };
        const SDL_Rect destination = { .x = v->area.x, .y = v->area.y, .w = w, .h = h };
        SDL_RenderCopy(renderer, v->text_texture, &source, &destination);
    }

    // Der Cursor leuchtet beim Tippen durchgehend und blinkt, wenn alles da ist.
    if(v->cursor_texture == NULL)
    {
        return;
    }
    if(count == v->character_count && (now / RAD_TYPEWRITER_CURSOR_BLINK_MS) % 2 == 1)
    {
        return;
    }

    int cursor_w = 0, cursor_h = 0;
    SDL_QueryTexture(v->cursor_texture, NULL, NULL, &cursor_w, &cursor_h);
    const int space = v->area.w - v->visible_width;
    if(space <= 0)
    {
        return;
    }
    if(cursor_w > space)
    {
        cursor_w = space;
    }
    if(cursor_h > v->area.h)
    {
        cursor_h = v->area.h;
    }

    const SDL_Rect cursor_source = { .x = 0, .y = 0, .w = cursor_w, .h = cursor_h };
    const SDL_Rect cursor_destination = { .x = v->area.x + v->visible_width, .y = v->area.y, .w = cursor_w, .h = cursor_h };
    SDL_RenderCopy(renderer, v->cursor_texture, &cursor_source, &cursor_destination);
}


static void RAD_TypewriterViewBuildTextures(RAD_TypewriterView_t *typewriter_view, SDL_Renderer *renderer)
{
    RAD_TypewriterViewDestroyTextures(typewriter_view);

    // Ohne Schrift oder Text gibt es nichts zu zeichnen, auch keinen Cursor.
    if(typewriter_view->font == NULL || typewriter_view->text == NULL)
    {
        return;
    }

    typewriter_view->text_texture = RAD_TypewriterViewRender(typewriter_view->font, typewriter_view->text,
                                                             typewriter_view->color, renderer);
    typewriter_view->cursor_texture = RAD_TypewriterViewRender(typewriter_view->font, RAD_TYPEWRITER_CURSOR,
                                                               typewriter_view->color, renderer);
}

static void RAD_TypewriterViewDestroyTextures(RAD_TypewriterView_t *typewriter_view)
{
    if(typewriter_view->text_texture != NULL)
    {
        SDL_DestroyTexture(typewriter_view->text_texture);
        typewriter_view->text_texture = NULL;
    }
    if(typewriter_view->cursor_texture != NULL)
    {
        SDL_DestroyTexture(typewriter_view->cursor_texture);
        typewriter_view->cursor_texture = NULL;
    }
}

static SDL_Texture* RAD_TypewriterViewRender(TTF_Font *font, const char *text, SDL_Color color, SDL_Renderer *renderer)
{
    SDL_Surface *surface = TTF_RenderUTF8_Blended(font, text, color);
    if(surface == NULL)
    {
        printf("TTF_RenderUTF8_Blended \"%s\": %s\n", text, TTF_GetError());
        return NULL;
    }

    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_FreeSurface(surface);
    if(texture == NULL)
    {
        printf("SDL_CreateTextureFromSurface: %s\n", SDL_GetError());
    }
    return texture;
}

///
/// Wie breit die ersten "count" Zeichen des Textes in der Schrift sind, in
/// Pixeln. Setzt dafuer kurz ein '\0' in die eigene Kopie des Textes.
///
static int32_t RAD_TypewriterViewMeasure(RAD_TypewriterView_t *typewriter_view, size_t count)
{
    if(count == 0)
    {
        return 0;
    }

    char *text = typewriter_view->text;
    const size_t offset = RAD_Utf8ByteOffset(text, count);
    const char kept = text[offset];
    text[offset] = '\0';

    int width = 0;
    if(TTF_SizeUTF8(typewriter_view->font, text, &width, NULL) != 0)
    {
        width = 0;
    }

    text[offset] = kept;
    return width;
}

///
/// Zaehlt die Zeichen eines UTF-8-Textes: jedes Byte, das kein
/// Folgebyte (10xxxxxx) ist, beginnt eines. 0 fuer NULL.
///
static size_t RAD_Utf8CharacterCount(const char *text)
{
    size_t count = 0;
    for(const char *c = text; c != NULL && *c != '\0'; c++)
    {
        if(((unsigned char)*c & 0xC0) != 0x80)
        {
            count++;
        }
    }
    return count;
}

///
/// Das Byte, an dem das Zeichen mit Index "count" beginnt -- oder das
/// abschliessende '\0', wenn der Text weniger Zeichen hat.
///
static size_t RAD_Utf8ByteOffset(const char *text, size_t count)
{
    size_t offset = 0;
    size_t seen = 0;
    while(text[offset] != '\0')
    {
        if(((unsigned char)text[offset] & 0xC0) != 0x80)
        {
            if(seen == count)
            {
                break;
            }
            seen++;
        }
        offset++;
    }
    return offset;
}
