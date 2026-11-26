#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <radish/view/rendering/iso_effect_sprite.h>

// Nur die Deklarationen; die Implementierung steht einmal in io/jsmn_impl.c.
#define JSMN_HEADER
#include <jsmn.h>

///
/// Die Tokens einer Datei, statisch wie bei den Lesern der sprite.json und der
/// tile.json -- der Heap ist tabu.
///
static jsmntok_t rad_iso_effect_sprite_tokens[RAD_ISO_EFFECT_SPRITE_TOKENS_MAX];

typedef struct
{
    const char *json;
    const jsmntok_t *tokens;
    int32_t number_of_tokens;
} RAD_IsoEffectSpriteTokens_t;

///
/// Die Richtungen, wie sie in "facings" heissen, nach RAD_IsoFacing_t -- wie in
/// der sprite.json (iso_unit_sprite.c).
///
static const char *const RAD_ISO_EFFECT_FACING_NAMES[RAD_ISO_FACINGS] = { "sw", "se", "ne", "nw" };

///
/// "kind" nach RAD_IsoEffectKind_t.
///
static const char *const RAD_ISO_EFFECT_KIND_NAMES[RAD_ISO_EFFECT_KINDS] = { "explosion" };

/// Die Pflichtschluessel eines Objekts als Bits, wie in iso_unit_sprite.c.
#define RAD_FOUND(bit) (1u << (bit))

const char* RAD_IsoEffectSpriteResultText(RAD_IsoEffectSpriteResult_t result)
{
    switch(result)
    {
        case RAD_ISO_EFFECT_SPRITE_OK:                    return "in Ordnung";
        case RAD_ISO_EFFECT_SPRITE_ERROR_TOO_LARGE:       return "zu gross";
        case RAD_ISO_EFFECT_SPRITE_ERROR_SYNTAX:          return "kein gueltiges JSON";
        case RAD_ISO_EFFECT_SPRITE_ERROR_TOO_MANY_TOKENS: return "zu viele Eintraege";
        case RAD_ISO_EFFECT_SPRITE_ERROR_SCHEMA:          return "nicht nach effect.schema.json";
        default:                                          return "unbekanntes Ergebnis";
    }
}

// ---------------------------------------------------------------------------
// Tokens lesen
// ---------------------------------------------------------------------------

static bool RAD_IsoEffectIsType(const RAD_IsoEffectSpriteTokens_t *t, int32_t index, jsmntype_t type)
{
    return (index >= 0) && (index < t->number_of_tokens) && (t->tokens[index].type == type);
}

///
/// Der Index hinter dem Wert an "index" samt allem, was darin verschachtelt ist.
/// Die Datei ist flach (hoechstens vier Ebenen).
///
static int32_t RAD_IsoEffectSubtreeEnd(const RAD_IsoEffectSpriteTokens_t *t, int32_t index)
{
    if((index < 0) || (index >= t->number_of_tokens))
    {
        return t->number_of_tokens;
    }

    int32_t next = index + 1;
    for(int32_t i = 0; i < t->tokens[index].size; ++i)
    {
        next = RAD_IsoEffectSubtreeEnd(t, next);
    }
    return next;
}

static int32_t RAD_IsoEffectTokenLength(const RAD_IsoEffectSpriteTokens_t *t, int32_t index)
{
    return (int32_t)(t->tokens[index].end - t->tokens[index].start);
}

static const char* RAD_IsoEffectTokenText(const RAD_IsoEffectSpriteTokens_t *t, int32_t index)
{
    return t->json + t->tokens[index].start;
}

static bool RAD_IsoEffectKeyIs(const RAD_IsoEffectSpriteTokens_t *t, int32_t index, const char *key)
{
    const size_t length = strlen(key);
    return RAD_IsoEffectIsType(t, index, JSMN_STRING)
           && ((size_t)RAD_IsoEffectTokenLength(t, index) == length)
           && (memcmp(RAD_IsoEffectTokenText(t, index), key, length) == 0);
}

///
/// Der Index von "key" in "names" (count Eintraege), oder -1.
///
static int32_t RAD_IsoEffectKeyIndex(const RAD_IsoEffectSpriteTokens_t *t, int32_t index,
                                     const char *const *names, int32_t count)
{
    for(int32_t i = 0; i < count; ++i)
    {
        if(RAD_IsoEffectKeyIs(t, index, names[i]))
        {
            return i;
        }
    }
    return -1;
}

///
/// Eine ganze Zahl in [min, max], ohne Vorzeichen, Nachkommastellen und
/// Exponent -- hoechstens vier Ziffern, mehr braucht keine Grenze hier.
///
static bool RAD_IsoEffectReadInt(const RAD_IsoEffectSpriteTokens_t *t, int32_t index, int32_t min, int32_t max,
                                 int32_t *out)
{
    if(!RAD_IsoEffectIsType(t, index, JSMN_PRIMITIVE))
    {
        return false;
    }

    const char *text = RAD_IsoEffectTokenText(t, index);
    const int32_t length = RAD_IsoEffectTokenLength(t, index);
    if((length < 1) || (length > 4))
    {
        return false;
    }

    int32_t value = 0;
    for(int32_t i = 0; i < length; ++i)
    {
        if((text[i] < '0') || (text[i] > '9'))
        {
            return false;
        }
        value = value * 10 + (text[i] - '0');
    }
    if((value < min) || (value > max))
    {
        return false;
    }
    *out = value;
    return true;
}

///
/// Eine Zahl, auch mit Nachkommastellen, in [min, max]; "exclusive_min": echt
/// groesser als "min". strtod braucht eine Null am Ende, deshalb ein kleiner
/// Puffer -- laengere Zahlen sind hier ohnehin falsch.
///
static bool RAD_IsoEffectReadNumber(const RAD_IsoEffectSpriteTokens_t *t, int32_t index, float min,
                                    bool exclusive_min, float max, float *out)
{
    char buffer[32];
    if(!RAD_IsoEffectIsType(t, index, JSMN_PRIMITIVE))
    {
        return false;
    }

    const int32_t length = RAD_IsoEffectTokenLength(t, index);
    const char *text = RAD_IsoEffectTokenText(t, index);
    // true, false und null fangen nicht mit einer Ziffer oder "-" an.
    if((length < 1) || (length >= (int32_t)sizeof(buffer))
       || !(((text[0] >= '0') && (text[0] <= '9')) || (text[0] == '-')))
    {
        return false;
    }
    memcpy(buffer, text, (size_t)length);
    buffer[length] = '\0';

    char *end = NULL;
    const double value = strtod(buffer, &end);
    if((end != buffer + length) || !isfinite(value))
    {
        return false;
    }
    const bool above_min = exclusive_min ? (value > (double)min) : (value >= (double)min);
    if(!above_min || (value > (double)max))
    {
        return false;
    }
    *out = (float)value;
    return true;
}

static bool RAD_IsoEffectIsKeyCharacter(char c)
{
    return ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) || (c == '_');
}

///
/// Ein Dateiname nach "^[a-z0-9_]{1,27}\.png$". Weil nur diese Zeichen gehen,
/// gibt es keine Maskierung, die der rohe Ausschnitt verfaelschen koennte.
///
static bool RAD_IsoEffectReadFileName(const RAD_IsoEffectSpriteTokens_t *t, int32_t index,
                                      char out[RAD_ISO_EFFECT_FILE_NAME_MAX])
{
    static const char extension[] = ".png";
    const int32_t extension_length = (int32_t)(sizeof(extension) - 1);

    if(!RAD_IsoEffectIsType(t, index, JSMN_STRING))
    {
        return false;
    }
    const char *text = RAD_IsoEffectTokenText(t, index);
    const int32_t length = RAD_IsoEffectTokenLength(t, index);
    const int32_t stem = length - extension_length;
    if((stem < 1) || (length >= RAD_ISO_EFFECT_FILE_NAME_MAX)
       || (memcmp(text + stem, extension, (size_t)extension_length) != 0))
    {
        return false;
    }
    for(int32_t i = 0; i < stem; ++i)
    {
        if(!RAD_IsoEffectIsKeyCharacter(text[i]))
        {
            return false;
        }
    }
    memcpy(out, text, (size_t)length);
    out[length] = '\0';
    return true;
}

// ---------------------------------------------------------------------------
// Die Ebenen der Datei
// ---------------------------------------------------------------------------

///
/// "anchor": x und y, beide Pflicht, je in [0, 1].
///
static bool RAD_IsoEffectReadAnchor(const RAD_IsoEffectSpriteTokens_t *t, int32_t index, RAD_IsoEffectAtlasInfo_t *atlas)
{
    enum { X, Y };
    if(!RAD_IsoEffectIsType(t, index, JSMN_OBJECT))
    {
        return false;
    }

    uint32_t found = 0;
    bool ok = true;
    int32_t key = index + 1;
    for(int32_t i = 0; ok && (i < t->tokens[index].size); ++i)
    {
        const int32_t value = key + 1;
        if(RAD_IsoEffectKeyIs(t, key, "x"))
        {
            ok = RAD_IsoEffectReadNumber(t, value, 0.0f, false, 1.0f, &atlas->anchor_x);
            found |= RAD_FOUND(X);
        }
        else if(RAD_IsoEffectKeyIs(t, key, "y"))
        {
            ok = RAD_IsoEffectReadNumber(t, value, 0.0f, false, 1.0f, &atlas->anchor_y);
            found |= RAD_FOUND(Y);
        }
        else
        {
            ok = false;
        }
        key = RAD_IsoEffectSubtreeEnd(t, key);
    }
    return ok && (found == (RAD_FOUND(X) | RAD_FOUND(Y)));
}

///
/// Die Folge einer Richtung ($defs/sequence): alle Felder Pflicht.
///
static bool RAD_IsoEffectReadAtlas(const RAD_IsoEffectSpriteTokens_t *t, int32_t index, RAD_IsoEffectAtlasInfo_t *atlas)
{
    enum { FILE_NAME, FRAME_WIDTH, FRAME_HEIGHT, FIRST_FRAME, FRAME_COUNT, ANCHOR, REQUIRED };
    if(!RAD_IsoEffectIsType(t, index, JSMN_OBJECT))
    {
        return false;
    }

    uint32_t found = 0;
    bool ok = true;
    int32_t key = index + 1;
    for(int32_t i = 0; ok && (i < t->tokens[index].size); ++i)
    {
        const int32_t value = key + 1;
        if(RAD_IsoEffectKeyIs(t, key, "file"))
        {
            ok = RAD_IsoEffectReadFileName(t, value, atlas->file);
            found |= RAD_FOUND(FILE_NAME);
        }
        else if(RAD_IsoEffectKeyIs(t, key, "frame_width"))
        {
            ok = RAD_IsoEffectReadInt(t, value, 1, RAD_ISO_EFFECT_FRAME_SIZE_MAX, &atlas->frame_width);
            found |= RAD_FOUND(FRAME_WIDTH);
        }
        else if(RAD_IsoEffectKeyIs(t, key, "frame_height"))
        {
            ok = RAD_IsoEffectReadInt(t, value, 1, RAD_ISO_EFFECT_FRAME_SIZE_MAX, &atlas->frame_height);
            found |= RAD_FOUND(FRAME_HEIGHT);
        }
        else if(RAD_IsoEffectKeyIs(t, key, "first_frame"))
        {
            ok = RAD_IsoEffectReadInt(t, value, 0, RAD_ISO_EFFECT_FIRST_FRAME_MAX, &atlas->first_frame);
            found |= RAD_FOUND(FIRST_FRAME);
        }
        else if(RAD_IsoEffectKeyIs(t, key, "frame_count"))
        {
            ok = RAD_IsoEffectReadInt(t, value, 1, RAD_ISO_EFFECT_FRAMES_MAX, &atlas->frame_count);
            found |= RAD_FOUND(FRAME_COUNT);
        }
        else if(RAD_IsoEffectKeyIs(t, key, "anchor"))
        {
            ok = RAD_IsoEffectReadAnchor(t, value, atlas);
            found |= RAD_FOUND(ANCHOR);
        }
        else
        {
            ok = false;
        }
        key = RAD_IsoEffectSubtreeEnd(t, key);
    }
    return ok && (found == RAD_FOUND(REQUIRED) - 1u);
}

///
/// "facings": je Richtung eine Folge, alle vier Pflicht.
///
static bool RAD_IsoEffectReadFacings(const RAD_IsoEffectSpriteTokens_t *t, int32_t index, RAD_IsoEffectSpriteInfo_t *info)
{
    if(!RAD_IsoEffectIsType(t, index, JSMN_OBJECT))
    {
        return false;
    }

    uint32_t found = 0;
    bool ok = true;
    int32_t key = index + 1;
    for(int32_t i = 0; ok && (i < t->tokens[index].size); ++i)
    {
        const int32_t facing = RAD_IsoEffectKeyIndex(t, key, RAD_ISO_EFFECT_FACING_NAMES, RAD_ISO_FACINGS);
        if(facing < 0)
        {
            return false;
        }
        // Kommt eine Richtung zweimal vor, gilt die letzte -- ganz, nicht ergaenzt.
        RAD_IsoEffectAtlasInfo_t atlas;
        memset(&atlas, 0, sizeof(atlas));
        ok = RAD_IsoEffectReadAtlas(t, key + 1, &atlas);
        info->atlases[facing] = atlas;
        found |= RAD_FOUND(facing);
        key = RAD_IsoEffectSubtreeEnd(t, key);
    }
    return ok && (found == RAD_FOUND(RAD_ISO_FACINGS) - 1u);
}

///
/// Die Wurzel: alle Felder Pflicht.
///
static bool RAD_IsoEffectReadRoot(const RAD_IsoEffectSpriteTokens_t *t, RAD_IsoEffectSpriteInfo_t *info)
{
    enum { VERSION, KIND, DRAW_WIDTH, DURATION, FACINGS, REQUIRED };
    if(!RAD_IsoEffectIsType(t, 0, JSMN_OBJECT))
    {
        return false;
    }

    uint32_t found = 0;
    bool ok = true;
    int32_t key = 1;
    for(int32_t i = 0; ok && (i < t->tokens[0].size); ++i)
    {
        const int32_t value = key + 1;
        if(RAD_IsoEffectKeyIs(t, key, "version"))
        {
            int32_t version = 0;
            ok = RAD_IsoEffectReadInt(t, value, 1, 1, &version);
            found |= RAD_FOUND(VERSION);
        }
        else if(RAD_IsoEffectKeyIs(t, key, "kind"))
        {
            const int32_t kind = RAD_IsoEffectKeyIndex(t, value, RAD_ISO_EFFECT_KIND_NAMES, RAD_ISO_EFFECT_KINDS);
            ok = (kind >= 0);
            info->kind = (RAD_IsoEffectKind_t)kind;
            found |= RAD_FOUND(KIND);
        }
        else if(RAD_IsoEffectKeyIs(t, key, "draw_width"))
        {
            ok = RAD_IsoEffectReadInt(t, value, 1, RAD_ISO_EFFECT_DRAW_WIDTH_MAX, &info->draw_width);
            found |= RAD_FOUND(DRAW_WIDTH);
        }
        else if(RAD_IsoEffectKeyIs(t, key, "duration_ms"))
        {
            ok = RAD_IsoEffectReadNumber(t, value, 0.0f, true, RAD_ISO_EFFECT_DURATION_MS_MAX, &info->duration_ms);
            found |= RAD_FOUND(DURATION);
        }
        else if(RAD_IsoEffectKeyIs(t, key, "facings"))
        {
            ok = RAD_IsoEffectReadFacings(t, value, info);
            found |= RAD_FOUND(FACINGS);
        }
        else
        {
            ok = false;
        }
        key = RAD_IsoEffectSubtreeEnd(t, key);
    }
    return ok && (found == RAD_FOUND(REQUIRED) - 1u);
}

RAD_IsoEffectSpriteResult_t RAD_IsoEffectSpriteParse(const char *json, size_t length, RAD_IsoEffectSpriteInfo_t *out)
{
    if((json == NULL) || (out == NULL))
    {
        return RAD_ISO_EFFECT_SPRITE_ERROR_SYNTAX;
    }
    if(length > RAD_ISO_EFFECT_SPRITE_JSON_MAX)
    {
        return RAD_ISO_EFFECT_SPRITE_ERROR_TOO_LARGE;
    }

    jsmn_parser parser;
    jsmn_init(&parser);
    const int count = jsmn_parse(&parser, json, length, rad_iso_effect_sprite_tokens, RAD_ISO_EFFECT_SPRITE_TOKENS_MAX);
    if(count == JSMN_ERROR_NOMEM)
    {
        return RAD_ISO_EFFECT_SPRITE_ERROR_TOO_MANY_TOKENS;
    }
    if(count < 1)
    {
        return RAD_ISO_EFFECT_SPRITE_ERROR_SYNTAX;
    }

    const RAD_IsoEffectSpriteTokens_t tokens = {
        .json = json,
        .tokens = rad_iso_effect_sprite_tokens,
        .number_of_tokens = count
    };

    // Erst in eine eigene Beschreibung: bei einem Fehler bleibt "out", wie es war.
    RAD_IsoEffectSpriteInfo_t info;
    memset(&info, 0, sizeof(info));
    if(!RAD_IsoEffectReadRoot(&tokens, &info))
    {
        return RAD_ISO_EFFECT_SPRITE_ERROR_SCHEMA;
    }
    *out = info;
    return RAD_ISO_EFFECT_SPRITE_OK;
}

bool RAD_IsoEffectSpriteKeyValid(const char *key)
{
    if(key == NULL)
    {
        return false;
    }
    size_t length = 0;
    for(; key[length] != '\0'; ++length)
    {
        if((length >= RAD_ISO_EFFECT_KEY_MAX - 1) || !RAD_IsoEffectIsKeyCharacter(key[length]))
        {
            return false;
        }
    }
    return length > 0;
}

bool RAD_IsoEffectSpritePath(const char *key, const char *file, char *out, size_t size)
{
    if((out == NULL) || (size == 0))
    {
        return false;
    }
    out[0] = '\0';
    if(!RAD_IsoEffectSpriteKeyValid(key) || (file == NULL) || (file[0] == '\0'))
    {
        return false;
    }

    static const char prefix[] = "/assets/effects/";
    const size_t needed = (sizeof(prefix) - 1) + strlen(key) + 1 + strlen(file) + 1;
    if(needed > size)
    {
        return false;
    }
    strcpy(out, prefix);
    strcat(out, key);
    strcat(out, "/");
    strcat(out, file);
    return true;
}
