#include <radish/rendering/font.h>
#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


///
/// So viele Zeichen kennt eine Schrift hoechstens: die ersten 256 Codepunkte.
/// Was die .fnt darueber hinaus beschreibt, wird uebergangen.
///
#define RAD_FONT_GLYPHS 256

///
/// Dieses Zeichen steht fuer jedes, das die Schrift nicht hat.
///
#define RAD_FONT_FALLBACK '?'

///
/// Laengste Zeile, die aus der .fnt gelesen wird, samt '\0'. Eine "char"-Zeile
/// ist kaum 150 Zeichen lang.
///
#define RAD_FONT_LINE_MAX 512


///
/// Ein Zeichen, wie die .fnt es beschreibt, in Pixeln des Atlas: wo es im
/// Atlas steht (x, y, width, height), wo es relativ zur Stiftposition und zur
/// Oberkante der Zeile hinkommt (x_offset, y_offset), und wie weit der Stift
/// danach weiterrueckt (x_advance).
///
typedef struct
{
    bool present;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    int32_t x_offset;
    int32_t y_offset;
    int32_t x_advance;
} RAD_FontGlyph_t;

///
/// Unterschneidung eines Zeichenpaares, in Pixeln des Atlas.
///
typedef struct
{
    uint32_t first;
    uint32_t second;
    int32_t amount;
} RAD_FontKerning_t;

///
/// Die Struktur steht hier und nicht im Header: die Views sollen nicht wissen,
/// dass es ein Atlas ist.
///
struct RAD_Font
{
    SDL_Texture *atlas;

    ///
    /// Pixel auf dem Bildschirm je Pixel im Atlas: gewuenschte Groesse durch
    /// die Groesse, in der der Atlas gezeichnet ist ("info size").
    ///
    double scale;

    ///
    /// Zeilenhoehe im Atlas ("common lineHeight").
    ///
    int32_t line_height;

    RAD_FontGlyph_t glyphs[RAD_FONT_GLYPHS];

    ///
    /// Nach (first, second) sortiert, fuer bsearch.
    ///
    RAD_FontKerning_t *kernings;
    size_t kerning_count;
};


static bool RAD_FontParse(RAD_Font_t *font, FILE *file, int32_t *size, char *page, size_t page_size);
static bool RAD_FontReadInt(const char *line, const char *key, int32_t *value);
static bool RAD_FontReadString(const char *line, const char *key, char *value, size_t value_size);
static bool RAD_FontStartsWith(const char *line, const char *keyword);
static bool RAD_FontAddKerning(RAD_Font_t *font, size_t *capacity, const RAD_FontKerning_t *kerning);
static int RAD_FontCompareKerning(const void *a, const void *b);
static int32_t RAD_FontKerningAmount(const RAD_Font_t *font, uint32_t first, uint32_t second);
static const RAD_FontGlyph_t* RAD_FontGlyph(const RAD_Font_t *font, uint32_t *codepoint);
static uint32_t RAD_FontNextCodepoint(const char *text, size_t length, size_t *offset);
static char* RAD_FontAtlasPath(const char *fnt_path, const char *page);


RAD_Font_t* RAD_CreateFont(SDL_Renderer *renderer, const char *fnt_path, int32_t pixel_size)
{
    if(pixel_size <= 0)
    {
        printf("RAD_CreateFont %s: Groesse %d\n", fnt_path, (int)pixel_size);
        return NULL;
    }

    FILE *file = fopen(fnt_path, "r");
    if(file == NULL)
    {
        printf("RAD_CreateFont %s: nicht lesbar\n", fnt_path);
        return NULL;
    }

    RAD_Font_t *font = calloc(1, sizeof(struct RAD_Font));
    if(font == NULL)
    {
        fclose(file);
        return NULL;
    }

    int32_t size = 0;
    char page[RAD_FONT_LINE_MAX] = { 0 };
    const bool parsed = RAD_FontParse(font, file, &size, page, sizeof(page));
    fclose(file);
    if(!parsed || size <= 0 || font->line_height <= 0 || page[0] == '\0')
    {
        printf("RAD_CreateFont %s: keine gueltige BMFont-Datei (Textformat)\n", fnt_path);
        RAD_DestroyFont(&font);
        return NULL;
    }
    font->scale = (double)pixel_size / (double)size;

    char *atlas_path = RAD_FontAtlasPath(fnt_path, page);
    if(atlas_path == NULL)
    {
        RAD_DestroyFont(&font);
        return NULL;
    }
    font->atlas = IMG_LoadTexture(renderer, atlas_path);
    if(font->atlas == NULL)
    {
        printf("IMG_LoadTexture %s: %s\n", atlas_path, IMG_GetError());
        free(atlas_path);
        RAD_DestroyFont(&font);
        return NULL;
    }
    free(atlas_path);

    // Der Atlas ist weiss auf transparent: eingefaerbt wird beim Zeichnen
    // (SDL_SetTextureColorMod). Verkleinert wird er linear gefiltert, sonst
    // fransen die Kanten aus.
    SDL_SetTextureBlendMode(font->atlas, SDL_BLENDMODE_BLEND);
    SDL_SetTextureScaleMode(font->atlas, (pixel_size == size) ? SDL_ScaleModeNearest : SDL_ScaleModeLinear);

    if(font->kerning_count > 1)
    {
        qsort(font->kernings, font->kerning_count, sizeof(RAD_FontKerning_t), RAD_FontCompareKerning);
    }

    return font;
}

void RAD_DestroyFont(RAD_Font_t **font)
{
    RAD_Font_t *f = *font;

    if(f->atlas != NULL)
    {
        SDL_DestroyTexture(f->atlas);
    }
    free(f->kernings);
    free(f);
    *font = NULL;
}

int32_t RAD_FontLineHeight(const RAD_Font_t *font)
{
    return (int32_t)lround(font->line_height * font->scale);
}

int32_t RAD_FontMeasure(const RAD_Font_t *font, const char *text, size_t length)
{
    if(text == NULL)
    {
        return 0;
    }

    // Gerechnet wird in Pixeln des Atlas und erst am Ende skaliert -- genauso
    // wie in RAD_FontDraw, damit Breite und Gezeichnetes zusammenpassen.
    int64_t pen = 0;
    uint32_t previous = 0;
    size_t offset = 0;
    while(offset < length && text[offset] != '\0')
    {
        uint32_t codepoint = RAD_FontNextCodepoint(text, length, &offset);
        const RAD_FontGlyph_t *glyph = RAD_FontGlyph(font, &codepoint);
        if(glyph == NULL)
        {
            continue;
        }
        if(previous != 0)
        {
            pen += RAD_FontKerningAmount(font, previous, codepoint);
        }
        pen += glyph->x_advance;
        previous = codepoint;
    }

    return (int32_t)lround((double)pen * font->scale);
}

void RAD_FontDraw(const RAD_Font_t *font, SDL_Renderer *renderer,
                  const char *text, size_t length,
                  int32_t x, int32_t y, SDL_Color color, const SDL_Rect *clip)
{
    if(text == NULL)
    {
        return;
    }

    SDL_SetTextureColorMod(font->atlas, color.r, color.g, color.b);
    SDL_SetTextureAlphaMod(font->atlas, color.a);

    const double scale = font->scale;
    int64_t pen = 0;
    uint32_t previous = 0;
    size_t offset = 0;
    while(offset < length && text[offset] != '\0')
    {
        uint32_t codepoint = RAD_FontNextCodepoint(text, length, &offset);
        const RAD_FontGlyph_t *glyph = RAD_FontGlyph(font, &codepoint);
        if(glyph == NULL)
        {
            continue;
        }
        if(previous != 0)
        {
            pen += RAD_FontKerningAmount(font, previous, codepoint);
        }
        previous = codepoint;

        // Beide Kanten gerundet statt Ecke plus gerundete Breite: so stossen
        // benachbarte Zeichen ohne Luecke aneinander.
        const int64_t left = pen + glyph->x_offset;
        SDL_Rect destination = {
            .x = x + (int32_t)lround((double)left * scale),
            .y = y + (int32_t)lround((double)glyph->y_offset * scale)
        };
        destination.w = x + (int32_t)lround((double)(left + glyph->width) * scale) - destination.x;
        destination.h = y + (int32_t)lround((double)(glyph->y_offset + glyph->height) * scale) - destination.y;
        pen += glyph->x_advance;

        if(destination.w <= 0 || destination.h <= 0)
        {
            continue;
        }

        SDL_Rect source = { .x = glyph->x, .y = glyph->y, .w = glyph->width, .h = glyph->height };
        if(clip != NULL)
        {
            SDL_Rect visible;
            if(!SDL_IntersectRect(&destination, clip, &visible))
            {
                continue;
            }
            // Vom Quellrechteck denselben Anteil abschneiden wie vom Ziel.
            const double per_x = (double)glyph->width / (double)destination.w;
            const double per_y = (double)glyph->height / (double)destination.h;
            const int32_t source_left = glyph->x + (int32_t)lround((visible.x - destination.x) * per_x);
            const int32_t source_right = glyph->x + (int32_t)lround((visible.x + visible.w - destination.x) * per_x);
            const int32_t source_top = glyph->y + (int32_t)lround((visible.y - destination.y) * per_y);
            const int32_t source_bottom = glyph->y + (int32_t)lround((visible.y + visible.h - destination.y) * per_y);
            source = (SDL_Rect){ .x = source_left, .y = source_top, .w = source_right - source_left, .h = source_bottom - source_top };
            destination = visible;
            if(source.w <= 0 || source.h <= 0)
            {
                continue;
            }
        }

        SDL_RenderCopy(renderer, font->atlas, &source, &destination);
    }
}


///
/// Liest die .fnt Zeile fuer Zeile: "info size", "common lineHeight", den
/// Dateinamen der ersten "page", jedes "char" und jedes "kerning". Andere
/// Zeilen und Felder werden uebergangen. false, wenn kein Speicher da ist.
///
static bool RAD_FontParse(RAD_Font_t *font, FILE *file, int32_t *size, char *page, size_t page_size)
{
    char line[RAD_FONT_LINE_MAX];
    size_t kerning_capacity = 0;

    while(fgets(line, sizeof(line), file) != NULL)
    {
        if(RAD_FontStartsWith(line, "info"))
        {
            RAD_FontReadInt(line, "size", size);
        }
        else if(RAD_FontStartsWith(line, "common"))
        {
            RAD_FontReadInt(line, "lineHeight", &font->line_height);
        }
        else if(RAD_FontStartsWith(line, "page"))
        {
            int32_t id = -1;
            if(RAD_FontReadInt(line, "id", &id) && id == 0)
            {
                RAD_FontReadString(line, "file", page, page_size);
            }
        }
        else if(RAD_FontStartsWith(line, "char"))
        {
            int32_t id = -1;
            RAD_FontGlyph_t glyph = { .present = true };
            if(RAD_FontReadInt(line, "id", &id)
               && RAD_FontReadInt(line, "x", &glyph.x)
               && RAD_FontReadInt(line, "y", &glyph.y)
               && RAD_FontReadInt(line, "width", &glyph.width)
               && RAD_FontReadInt(line, "height", &glyph.height)
               && RAD_FontReadInt(line, "xoffset", &glyph.x_offset)
               && RAD_FontReadInt(line, "yoffset", &glyph.y_offset)
               && RAD_FontReadInt(line, "xadvance", &glyph.x_advance)
               && id >= 0 && id < RAD_FONT_GLYPHS)
            {
                font->glyphs[id] = glyph;
            }
        }
        else if(RAD_FontStartsWith(line, "kerning"))
        {
            int32_t first = -1, second = -1, amount = 0;
            if(RAD_FontReadInt(line, "first", &first)
               && RAD_FontReadInt(line, "second", &second)
               && RAD_FontReadInt(line, "amount", &amount)
               && first >= 0 && first < RAD_FONT_GLYPHS
               && second >= 0 && second < RAD_FONT_GLYPHS
               && amount != 0)
            {
                const RAD_FontKerning_t kerning = { .first = (uint32_t)first, .second = (uint32_t)second, .amount = amount };
                if(!RAD_FontAddKerning(font, &kerning_capacity, &kerning))
                {
                    return false;
                }
            }
        }
    }

    return true;
}

///
/// Liest "key=<Zahl>" aus einer Zeile. Das Feld muss nach einem Leerzeichen
/// stehen, damit "x" nicht in "xoffset" gefunden wird.
///
static bool RAD_FontReadInt(const char *line, const char *key, int32_t *value)
{
    char pattern[32];
    snprintf(pattern, sizeof(pattern), " %s=", key);
    const char *found = strstr(line, pattern);
    if(found == NULL)
    {
        return false;
    }

    char *end = NULL;
    const long parsed = strtol(found + strlen(pattern), &end, 10);
    if(end == found + strlen(pattern))
    {
        return false;
    }
    *value = (int32_t)parsed;
    return true;
}

///
/// Liest "key=\"<Text>\"" aus einer Zeile, ohne die Anfuehrungszeichen.
///
static bool RAD_FontReadString(const char *line, const char *key, char *value, size_t value_size)
{
    char pattern[32];
    snprintf(pattern, sizeof(pattern), " %s=\"", key);
    const char *found = strstr(line, pattern);
    if(found == NULL)
    {
        return false;
    }

    const char *start = found + strlen(pattern);
    const char *end = strchr(start, '"');
    if(end == NULL || (size_t)(end - start) >= value_size)
    {
        return false;
    }
    memcpy(value, start, (size_t)(end - start));
    value[end - start] = '\0';
    return true;
}

///
/// Ob die Zeile mit dem Wort "keyword" beginnt: "char" passt auf
/// "char id=...", nicht aber auf "chars count=...".
///
static bool RAD_FontStartsWith(const char *line, const char *keyword)
{
    const size_t length = strlen(keyword);
    return strncmp(line, keyword, length) == 0 && (line[length] == ' ' || line[length] == '\t');
}

static bool RAD_FontAddKerning(RAD_Font_t *font, size_t *capacity, const RAD_FontKerning_t *kerning)
{
    if(font->kerning_count == *capacity)
    {
        const size_t grown = (*capacity == 0) ? 64 : *capacity * 2;
        RAD_FontKerning_t *kernings = realloc(font->kernings, grown * sizeof(RAD_FontKerning_t));
        if(kernings == NULL)
        {
            return false;
        }
        font->kernings = kernings;
        *capacity = grown;
    }
    font->kernings[font->kerning_count++] = *kerning;
    return true;
}

static int RAD_FontCompareKerning(const void *a, const void *b)
{
    const RAD_FontKerning_t *ka = a;
    const RAD_FontKerning_t *kb = b;
    if(ka->first != kb->first)
    {
        return (ka->first < kb->first) ? -1 : 1;
    }
    if(ka->second != kb->second)
    {
        return (ka->second < kb->second) ? -1 : 1;
    }
    return 0;
}

static int32_t RAD_FontKerningAmount(const RAD_Font_t *font, uint32_t first, uint32_t second)
{
    if(font->kerning_count == 0)
    {
        return 0;
    }
    const RAD_FontKerning_t key = { .first = first, .second = second };
    const RAD_FontKerning_t *found = bsearch(&key, font->kernings, font->kerning_count,
                                             sizeof(RAD_FontKerning_t), RAD_FontCompareKerning);
    return (found != NULL) ? found->amount : 0;
}

///
/// Das Zeichen fuer "codepoint", oder das Ersatzzeichen, wenn die Schrift es
/// nicht hat -- dann steht danach auch in "codepoint" das Ersatzzeichen, fuer
/// die Unterschneidung. NULL, wenn sie auch das nicht hat.
///
static const RAD_FontGlyph_t* RAD_FontGlyph(const RAD_Font_t *font, uint32_t *codepoint)
{
    if(*codepoint < RAD_FONT_GLYPHS && font->glyphs[*codepoint].present)
    {
        return &font->glyphs[*codepoint];
    }
    *codepoint = RAD_FONT_FALLBACK;
    return font->glyphs[RAD_FONT_FALLBACK].present ? &font->glyphs[RAD_FONT_FALLBACK] : NULL;
}

///
/// Liest ein UTF-8-Zeichen ab "offset" und rueckt "offset" dahinter. Ein
/// ungueltiges oder abgeschnittenes Zeichen zaehlt als ein Byte und wird zum
/// Ersatzzeichen.
///
static uint32_t RAD_FontNextCodepoint(const char *text, size_t length, size_t *offset)
{
    const unsigned char lead = (unsigned char)text[*offset];
    size_t count;
    uint32_t codepoint;

    if(lead < 0x80)
    {
        (*offset)++;
        return lead;
    }
    else if((lead & 0xE0) == 0xC0)
    {
        count = 1;
        codepoint = lead & 0x1F;
    }
    else if((lead & 0xF0) == 0xE0)
    {
        count = 2;
        codepoint = lead & 0x0F;
    }
    else if((lead & 0xF8) == 0xF0)
    {
        count = 3;
        codepoint = lead & 0x07;
    }
    else
    {
        (*offset)++;
        return RAD_FONT_FALLBACK;
    }

    for(size_t i = 1; i <= count; i++)
    {
        const size_t at = *offset + i;
        if(at >= length || ((unsigned char)text[at] & 0xC0) != 0x80)
        {
            (*offset)++;
            return RAD_FONT_FALLBACK;
        }
        codepoint = (codepoint << 6) | ((unsigned char)text[at] & 0x3F);
    }
    *offset += count + 1;
    return codepoint;
}

///
/// "page" relativ zum Verzeichnis von "fnt_path". Der Aufrufer gibt das
/// Ergebnis frei; NULL, wenn kein Speicher da ist.
///
static char* RAD_FontAtlasPath(const char *fnt_path, const char *page)
{
    const char *slash = strrchr(fnt_path, '/');
    const size_t directory_length = (slash != NULL) ? (size_t)(slash - fnt_path) + 1 : 0;
    const size_t page_length = strlen(page);

    char *path = malloc(directory_length + page_length + 1);
    if(path == NULL)
    {
        return NULL;
    }
    memcpy(path, fnt_path, directory_length);
    memcpy(path + directory_length, page, page_length + 1);
    return path;
}
