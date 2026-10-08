#include <radish/view/debug/debug_view.h>
#include <radish/view/text_view.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


///
/// Abstand zum Rand, zwischen den Zeilen und zwischen Schluessel und Wert, in
/// Pixeln.
///
#define RAD_DEBUG_VIEW_PADDING 4
#define RAD_DEBUG_VIEW_GAP 4
#define RAD_DEBUG_VIEW_COLUMN_GAP 12

///
/// Deckkraft des Hintergrunds, 0 bis 255: 40%, also zu 60% durchsichtig. Die
/// Schrift bleibt voll deckend.
///
#define RAD_DEBUG_VIEW_BACKGROUND_ALPHA 102

///
/// Platz fuer einen Wert als Text samt abschliessender Null.
///
#define RAD_DEBUG_VIEW_VALUE_MAX 32

///
/// Steht an Stelle eines Wertes, solange keiner gesetzt ist.
///
#define RAD_DEBUG_VIEW_NO_VALUE "-"


///
/// Die Paare, die es gibt, in der Reihenfolge von oben nach unten. Ein neues
/// Paar: hier einen Eintrag, in RAD_DEBUG_VIEW_KEYS seinen Schluessel und im
/// Header einen Setter.
///
typedef enum
{
    RAD_DEBUG_VIEW_ROW_AVG_FPS = 0,
    RAD_DEBUG_VIEW_ROW_AVG_UPDATE_MS,
    RAD_DEBUG_VIEW_ROW_MAX_UPDATE_MS,
    RAD_DEBUG_VIEW_ROW_COUNT
} RAD_DebugViewRow_t;

static const char *const RAD_DEBUG_VIEW_KEYS[RAD_DEBUG_VIEW_ROW_COUNT] = {
    [RAD_DEBUG_VIEW_ROW_AVG_FPS] = "avg fps",
    [RAD_DEBUG_VIEW_ROW_AVG_UPDATE_MS] = "avg update",
    [RAD_DEBUG_VIEW_ROW_MAX_UPDATE_MS] = "max update"
};


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_DebugView
{
    SDL_Rect area;

    ///
    /// Je Zeile Schluessel und Wert, und der Wert als Text, wie er gerade
    /// angezeigt wird -- zum Vergleichen in RAD_DebugViewSetValue.
    ///
    RAD_TextView_t *keys[RAD_DEBUG_VIEW_ROW_COUNT];
    RAD_TextView_t *values[RAD_DEBUG_VIEW_ROW_COUNT];
    char shown_values[RAD_DEBUG_VIEW_ROW_COUNT][RAD_DEBUG_VIEW_VALUE_MAX];
};


static void RAD_DebugViewSetValue(RAD_DebugView_t *debug_view, RAD_DebugViewRow_t row, const char *value);
static void RAD_DebugViewSetMs(RAD_DebugView_t *debug_view, RAD_DebugViewRow_t row, float ms);


RAD_DebugView_t* RAD_CreateDebugView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font)
{
    RAD_DebugView_t *debug_view = calloc(1, sizeof(struct RAD_DebugView));
    if(debug_view == NULL)
    {
        return NULL;
    }

    // So hoch wie alle Zeilen samt Rand, hoechstens "height", an der Oberkante
    // der Flaeche.
    const int32_t line = TTF_FontLineSkip(font);
    const int32_t step = line + RAD_DEBUG_VIEW_GAP;
    int32_t content_height = 2 * RAD_DEBUG_VIEW_PADDING + RAD_DEBUG_VIEW_ROW_COUNT * line + (RAD_DEBUG_VIEW_ROW_COUNT - 1) * RAD_DEBUG_VIEW_GAP;
    if(content_height > height)
    {
        content_height = height;
    }
    debug_view->area = (SDL_Rect){ .x = x, .y = y, .w = width, .h = content_height };

    // Die Werte stehen in einer Spalte rechts neben dem breitesten Schluessel.
    int32_t key_width = 0;
    for(int32_t row = 0; row < RAD_DEBUG_VIEW_ROW_COUNT; row++)
    {
        int w = 0;
        if(TTF_SizeUTF8(font, RAD_DEBUG_VIEW_KEYS[row], &w, NULL) == 0 && w > key_width)
        {
            key_width = w;
        }
    }

    const int32_t key_x = x + RAD_DEBUG_VIEW_PADDING;
    const int32_t value_x = key_x + key_width + RAD_DEBUG_VIEW_COLUMN_GAP;
    const int32_t value_width = x + width - RAD_DEBUG_VIEW_PADDING - value_x;

    for(int32_t row = 0; row < RAD_DEBUG_VIEW_ROW_COUNT; row++)
    {
        const int32_t row_y = y + RAD_DEBUG_VIEW_PADDING + row * step;
        debug_view->keys[row] = RAD_CreateTextView(key_x, row_y, key_width, line, font);
        debug_view->values[row] = RAD_CreateTextView(value_x, row_y, (value_width > 0) ? value_width : 0, line, font);
        if(debug_view->keys[row] == NULL || debug_view->values[row] == NULL
           || !RAD_TextViewSetText(debug_view->keys[row], RAD_DEBUG_VIEW_KEYS[row]))
        {
            RAD_DestroyDebugView(&debug_view);
            return NULL;
        }
        RAD_DebugViewSetValue(debug_view, (RAD_DebugViewRow_t)row, RAD_DEBUG_VIEW_NO_VALUE);
    }

    return debug_view;
}

///
/// Baut auch eine halb angelegte DebugView ab (RAD_CreateDebugView im
/// Fehlerfall): einzelne TextViews sind dann womoeglich NULL -- calloc.
///
void RAD_DestroyDebugView(RAD_DebugView_t **debug_view)
{
    RAD_DebugView_t *v = *debug_view;

    for(int32_t row = 0; row < RAD_DEBUG_VIEW_ROW_COUNT; row++)
    {
        if(v->keys[row] != NULL)
        {
            RAD_DestroyTextView(&v->keys[row]);
        }
        if(v->values[row] != NULL)
        {
            RAD_DestroyTextView(&v->values[row]);
        }
    }

    free(v);
    *debug_view = NULL;
}

void RAD_DebugViewSetAvgFps(RAD_DebugView_t *debug_view, float avg_fps)
{
    char value[RAD_DEBUG_VIEW_VALUE_MAX];
    snprintf(value, sizeof(value), "%.1f", (double)avg_fps);
    RAD_DebugViewSetValue(debug_view, RAD_DEBUG_VIEW_ROW_AVG_FPS, value);
}

void RAD_DebugViewSetAvgUpdateMs(RAD_DebugView_t *debug_view, float avg_update_ms)
{
    RAD_DebugViewSetMs(debug_view, RAD_DEBUG_VIEW_ROW_AVG_UPDATE_MS, avg_update_ms);
}

void RAD_DebugViewSetMaxUpdateMs(RAD_DebugView_t *debug_view, float max_update_ms)
{
    RAD_DebugViewSetMs(debug_view, RAD_DEBUG_VIEW_ROW_MAX_UPDATE_MS, max_update_ms);
}

void RAD_UpdateDebugView(RAD_DebugView_t *debug_view, SDL_Renderer *renderer)
{
    // Der Hintergrund mit Alpha gemischt, damit er durchscheint; danach den
    // Blend-Modus des Aufrufers wiederherstellen.
    SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &previous_blend_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 48, 48, 48, RAD_DEBUG_VIEW_BACKGROUND_ALPHA);
    SDL_RenderFillRect(renderer, &debug_view->area);
    SDL_SetRenderDrawBlendMode(renderer, previous_blend_mode);

    // Ist die Flaeche kleiner als der Inhalt, schneidet sie ihn ab; danach die
    // Clip-Flaeche des Aufrufers wiederherstellen.
    const SDL_bool was_clipping = SDL_RenderIsClipEnabled(renderer);
    SDL_Rect previous_clip;
    SDL_RenderGetClipRect(renderer, &previous_clip);
    SDL_RenderSetClipRect(renderer, &debug_view->area);

    for(int32_t row = 0; row < RAD_DEBUG_VIEW_ROW_COUNT; row++)
    {
        RAD_UpdateTextView(debug_view->keys[row], renderer);
        RAD_UpdateTextView(debug_view->values[row], renderer);
    }

    SDL_RenderSetClipRect(renderer, was_clipping ? &previous_clip : NULL);
}


///
/// Setzt den Wert in Zeile "row" -- nur, wenn er sich geaendert hat: jeder neue
/// Text muss neu gerendert werden. Fehlt dafuer der Speicher, bleibt der alte
/// stehen, und der naechste Aufruf versucht es neu.
///
static void RAD_DebugViewSetValue(RAD_DebugView_t *debug_view, RAD_DebugViewRow_t row, const char *value)
{
    if(strcmp(debug_view->shown_values[row], value) == 0)
    {
        return;
    }

    if(RAD_TextViewSetText(debug_view->values[row], value))
    {
        snprintf(debug_view->shown_values[row], sizeof(debug_view->shown_values[row]), "%s", value);
    }
}

///
/// Setzt den Wert in Zeile "row" auf eine Dauer: zwei Nachkommastellen und
/// "ms", z.B. "1.23 ms".
///
static void RAD_DebugViewSetMs(RAD_DebugView_t *debug_view, RAD_DebugViewRow_t row, float ms)
{
    char value[RAD_DEBUG_VIEW_VALUE_MAX];
    snprintf(value, sizeof(value), "%.2f ms", (double)ms);
    RAD_DebugViewSetValue(debug_view, row, value);
}
