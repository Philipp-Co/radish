#include <radish/view/typewriter_view.h>
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
    const RAD_Font_t *font;

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
    /// Wie viele Zeichen beim letzten Update sichtbar waren, wie viele Bytes
    /// sie sind und wie breit in der Schrift. Neu gemessen nur, wenn sich die
    /// Zahl aendert oder ein Setter sie veraltet (measure_stale).
    ///
    bool measure_stale;
    size_t visible_count;
    size_t visible_bytes;
    int32_t visible_width;
};


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
        .measure_stale = false,
        .visible_count = 0,
        .visible_bytes = 0,
        .visible_width = 0
    };

    return typewriter_view;
}

void RAD_DestroyTypewriterView(RAD_TypewriterView_t **typewriter_view)
{
    RAD_TypewriterView_t *v = *typewriter_view;

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
    typewriter_view->measure_stale = true;
    return true;
}

void RAD_TypewriterViewSetFont(RAD_TypewriterView_t *typewriter_view, const RAD_Font_t *font)
{
    typewriter_view->font = font;
    typewriter_view->measure_stale = true;
}

void RAD_TypewriterViewSetColor(RAD_TypewriterView_t *typewriter_view, SDL_Color color)
{
    typewriter_view->color = color;
}

int32_t RAD_TypewriterViewTextWidth(const RAD_TypewriterView_t *typewriter_view)
{
    if(typewriter_view->font == NULL || typewriter_view->text == NULL)
    {
        return 0;
    }

    return RAD_FontMeasure(typewriter_view->font, typewriter_view->text, strlen(typewriter_view->text))
           + RAD_FontMeasure(typewriter_view->font, RAD_TYPEWRITER_CURSOR, strlen(RAD_TYPEWRITER_CURSOR));
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

    // Ohne Schrift oder Text gibt es nichts zu zeichnen, auch keinen Cursor.
    if(v->font == NULL || v->text == NULL)
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
    if(v->measure_stale || count != v->visible_count)
    {
        v->measure_stale = false;
        v->visible_count = count;
        v->visible_bytes = RAD_Utf8ByteOffset(v->text, count);
        v->visible_width = RAD_FontMeasure(v->font, v->text, v->visible_bytes);
    }

    // Vom ganzen Text nur den getippten Anfang; was nicht in die Flaeche
    // passt, schneidet die Schrift ab.
    RAD_FontDraw(v->font, renderer, v->text, v->visible_bytes, v->area.x, v->area.y, v->color, &v->area);

    // Der Cursor leuchtet beim Tippen durchgehend und blinkt, wenn alles da ist.
    if(count == v->character_count && (now / RAD_TYPEWRITER_CURSOR_BLINK_MS) % 2 == 1)
    {
        return;
    }
    RAD_FontDraw(v->font, renderer, RAD_TYPEWRITER_CURSOR, strlen(RAD_TYPEWRITER_CURSOR),
                 v->area.x + v->visible_width, v->area.y, v->color, &v->area);
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
