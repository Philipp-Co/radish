#include <string.h>
#include <radish/view/rendering/iso_tile_sprite.h>

// Nur die Deklarationen; die Implementierung steht einmal in io/jsmn_impl.c.
#define JSMN_HEADER
#include <jsmn.h>

///
/// Die Tokens einer Datei, statisch wie beim Leser der sprite.json
/// (iso_unit_sprite.c) -- der Heap ist tabu.
///
static jsmntok_t rad_iso_tile_sprite_tokens[RAD_ISO_TILE_SPRITE_TOKENS_MAX];

typedef struct
{
    const char *json;
    const jsmntok_t *tokens;
    int32_t number_of_tokens;
} RAD_IsoTileSpriteTokens_t;

/// Die Pflichtschluessel eines Objekts als Bits, wie in iso_unit_sprite.c.
#define RAD_FOUND(bit) (1u << (bit))

const char* RAD_IsoTileSpriteResultText(RAD_IsoTileSpriteResult_t result)
{
    switch(result)
    {
        case RAD_ISO_TILE_SPRITE_OK:                    return "in Ordnung";
        case RAD_ISO_TILE_SPRITE_ERROR_TOO_LARGE:       return "zu gross";
        case RAD_ISO_TILE_SPRITE_ERROR_SYNTAX:          return "kein gueltiges JSON";
        case RAD_ISO_TILE_SPRITE_ERROR_TOO_MANY_TOKENS: return "zu viele Eintraege";
        case RAD_ISO_TILE_SPRITE_ERROR_SCHEMA:          return "nicht nach tile.schema.json";
        default:                                        return "unbekanntes Ergebnis";
    }
}

// ---------------------------------------------------------------------------
// Tokens lesen
// ---------------------------------------------------------------------------

static bool RAD_IsoTileIsType(const RAD_IsoTileSpriteTokens_t *t, int32_t index, jsmntype_t type)
{
    return (index >= 0) && (index < t->number_of_tokens) && (t->tokens[index].type == type);
}

///
/// Der Index hinter dem Wert an "index" samt allem, was darin verschachtelt ist.
/// Die Datei ist flach (hoechstens drei Ebenen).
///
static int32_t RAD_IsoTileSubtreeEnd(const RAD_IsoTileSpriteTokens_t *t, int32_t index)
{
    if((index < 0) || (index >= t->number_of_tokens))
    {
        return t->number_of_tokens;
    }

    int32_t next = index + 1;
    for(int32_t i = 0; i < t->tokens[index].size; ++i)
    {
        next = RAD_IsoTileSubtreeEnd(t, next);
    }
    return next;
}

static int32_t RAD_IsoTileTokenLength(const RAD_IsoTileSpriteTokens_t *t, int32_t index)
{
    return (int32_t)(t->tokens[index].end - t->tokens[index].start);
}

static const char* RAD_IsoTileTokenText(const RAD_IsoTileSpriteTokens_t *t, int32_t index)
{
    return t->json + t->tokens[index].start;
}

static bool RAD_IsoTileKeyIs(const RAD_IsoTileSpriteTokens_t *t, int32_t index, const char *key)
{
    const size_t length = strlen(key);
    return RAD_IsoTileIsType(t, index, JSMN_STRING)
           && ((size_t)RAD_IsoTileTokenLength(t, index) == length)
           && (memcmp(RAD_IsoTileTokenText(t, index), key, length) == 0);
}

///
/// Eine ganze Zahl in [min, max], wahlweise mit "-", ohne Nachkommastellen und
/// Exponent -- hoechstens vier Ziffern, mehr braucht keine Grenze hier.
///
static bool RAD_IsoTileReadInt(const RAD_IsoTileSpriteTokens_t *t, int32_t index, int32_t min, int32_t max, int32_t *out)
{
    if(!RAD_IsoTileIsType(t, index, JSMN_PRIMITIVE))
    {
        return false;
    }

    const char *text = RAD_IsoTileTokenText(t, index);
    int32_t length = RAD_IsoTileTokenLength(t, index);
    const bool negative = (length > 0) && (text[0] == '-');
    if(negative)
    {
        ++text;
        --length;
    }
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
    value = negative ? -value : value;
    if((value < min) || (value > max))
    {
        return false;
    }
    *out = value;
    return true;
}

static bool RAD_IsoTileReadBool(const RAD_IsoTileSpriteTokens_t *t, int32_t index, bool *out)
{
    if(!RAD_IsoTileIsType(t, index, JSMN_PRIMITIVE))
    {
        return false;
    }
    const char *text = RAD_IsoTileTokenText(t, index);
    const int32_t length = RAD_IsoTileTokenLength(t, index);
    if((length == 4) && (memcmp(text, "true", 4) == 0))
    {
        *out = true;
        return true;
    }
    if((length == 5) && (memcmp(text, "false", 5) == 0))
    {
        *out = false;
        return true;
    }
    return false;
}

static bool RAD_IsoTileIsKeyCharacter(char c)
{
    return ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) || (c == '_');
}

///
/// Ein Dateiname nach "^[a-z0-9_]{1,27}\.png$". Weil nur diese Zeichen gehen,
/// gibt es keine Maskierung, die der rohe Ausschnitt verfaelschen koennte.
///
static bool RAD_IsoTileReadFileName(const RAD_IsoTileSpriteTokens_t *t, int32_t index, char out[RAD_ISO_TILE_FILE_NAME_MAX])
{
    static const char extension[] = ".png";
    const int32_t extension_length = (int32_t)(sizeof(extension) - 1);

    if(!RAD_IsoTileIsType(t, index, JSMN_STRING))
    {
        return false;
    }
    const char *text = RAD_IsoTileTokenText(t, index);
    const int32_t length = RAD_IsoTileTokenLength(t, index);
    const int32_t stem = length - extension_length;
    if((stem < 1) || (length >= RAD_ISO_TILE_FILE_NAME_MAX)
       || (memcmp(text + stem, extension, (size_t)extension_length) != 0))
    {
        return false;
    }
    for(int32_t i = 0; i < stem; ++i)
    {
        if(!RAD_IsoTileIsKeyCharacter(text[i]))
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

static bool RAD_IsoTileReadOffset(const RAD_IsoTileSpriteTokens_t *t, int32_t index, int32_t *out)
{
    return RAD_IsoTileReadInt(t, index, -RAD_ISO_TILE_DECOR_OFFSET_MAX, RAD_ISO_TILE_DECOR_OFFSET_MAX, out);
}

///
/// Eine Dekoration ($defs/decor): Pflicht ist nur "image".
///
static bool RAD_IsoTileReadDecor(const RAD_IsoTileSpriteTokens_t *t, int32_t index, RAD_IsoTileDecorInfo_t *decor)
{
    if(!RAD_IsoTileIsType(t, index, JSMN_OBJECT))
    {
        return false;
    }

    bool has_image = false;
    bool ok = true;
    int32_t key = index + 1;
    for(int32_t i = 0; ok && (i < t->tokens[index].size); ++i)
    {
        const int32_t value = key + 1;
        if(RAD_IsoTileKeyIs(t, key, "image"))
        {
            ok = RAD_IsoTileReadFileName(t, value, decor->image);
            has_image = true;
        }
        else if(RAD_IsoTileKeyIs(t, key, "offset_x"))
        {
            ok = RAD_IsoTileReadOffset(t, value, &decor->offset_x);
        }
        else if(RAD_IsoTileKeyIs(t, key, "offset_y"))
        {
            ok = RAD_IsoTileReadOffset(t, value, &decor->offset_y);
        }
        else if(RAD_IsoTileKeyIs(t, key, "in_front"))
        {
            ok = RAD_IsoTileReadBool(t, value, &decor->in_front);
        }
        else
        {
            ok = false;
        }
        key = RAD_IsoTileSubtreeEnd(t, key);
    }
    return ok && has_image;
}

static bool RAD_IsoTileReadDecorList(const RAD_IsoTileSpriteTokens_t *t, int32_t index, RAD_IsoTileSpriteInfo_t *info)
{
    if(!RAD_IsoTileIsType(t, index, JSMN_ARRAY) || (t->tokens[index].size > RAD_ISO_TILE_DECOR_MAX))
    {
        return false;
    }

    // Kommt "decor" zweimal vor, gilt der letzte -- ganz, nicht ergaenzt.
    memset(info->decor, 0, sizeof(info->decor));
    info->number_of_decor = t->tokens[index].size;

    int32_t element = index + 1;
    for(int32_t i = 0; i < info->number_of_decor; ++i)
    {
        if(!RAD_IsoTileReadDecor(t, element, &info->decor[i]))
        {
            return false;
        }
        element = RAD_IsoTileSubtreeEnd(t, element);
    }
    return true;
}

///
/// Die Wurzel: Pflicht ist nur "version".
///
static bool RAD_IsoTileReadRoot(const RAD_IsoTileSpriteTokens_t *t, RAD_IsoTileSpriteInfo_t *info)
{
    if(!RAD_IsoTileIsType(t, 0, JSMN_OBJECT))
    {
        return false;
    }

    bool has_version = false;
    bool ok = true;
    int32_t key = 1;
    for(int32_t i = 0; ok && (i < t->tokens[0].size); ++i)
    {
        const int32_t value = key + 1;
        if(RAD_IsoTileKeyIs(t, key, "version"))
        {
            int32_t version = 0;
            ok = RAD_IsoTileReadInt(t, value, 1, 1, &version);
            has_version = true;
        }
        else if(RAD_IsoTileKeyIs(t, key, "image"))
        {
            ok = RAD_IsoTileReadFileName(t, value, info->image);
        }
        else if(RAD_IsoTileKeyIs(t, key, "decor"))
        {
            ok = RAD_IsoTileReadDecorList(t, value, info);
        }
        else
        {
            ok = false;
        }
        key = RAD_IsoTileSubtreeEnd(t, key);
    }
    return ok && has_version;
}

RAD_IsoTileSpriteResult_t RAD_IsoTileSpriteParse(const char *json, size_t length, RAD_IsoTileSpriteInfo_t *out)
{
    if((json == NULL) || (out == NULL))
    {
        return RAD_ISO_TILE_SPRITE_ERROR_SYNTAX;
    }
    if(length > RAD_ISO_TILE_SPRITE_JSON_MAX)
    {
        return RAD_ISO_TILE_SPRITE_ERROR_TOO_LARGE;
    }

    jsmn_parser parser;
    jsmn_init(&parser);
    const int count = jsmn_parse(&parser, json, length, rad_iso_tile_sprite_tokens, RAD_ISO_TILE_SPRITE_TOKENS_MAX);
    if(count == JSMN_ERROR_NOMEM)
    {
        return RAD_ISO_TILE_SPRITE_ERROR_TOO_MANY_TOKENS;
    }
    if(count < 1)
    {
        return RAD_ISO_TILE_SPRITE_ERROR_SYNTAX;
    }

    const RAD_IsoTileSpriteTokens_t tokens = {
        .json = json,
        .tokens = rad_iso_tile_sprite_tokens,
        .number_of_tokens = count
    };

    // Erst in eine eigene Beschreibung: bei einem Fehler bleibt "out", wie es war.
    RAD_IsoTileSpriteInfo_t info;
    memset(&info, 0, sizeof(info));
    if(!RAD_IsoTileReadRoot(&tokens, &info))
    {
        return RAD_ISO_TILE_SPRITE_ERROR_SCHEMA;
    }
    *out = info;
    return RAD_ISO_TILE_SPRITE_OK;
}

static bool RAD_IsoTileKeyValid(const char *key)
{
    size_t length = 0;
    for(; key[length] != '\0'; ++length)
    {
        if((length >= RAD_ISO_TILE_FILE_NAME_MAX - 1) || !RAD_IsoTileIsKeyCharacter(key[length]))
        {
            return false;
        }
    }
    return length > 0;
}

bool RAD_IsoTileSpritePath(const char *key, const char *file, char *out, size_t size)
{
    if((out == NULL) || (size == 0))
    {
        return false;
    }
    out[0] = '\0';
    if((key == NULL) || !RAD_IsoTileKeyValid(key) || (file == NULL) || (file[0] == '\0'))
    {
        return false;
    }

    static const char prefix[] = "/assets/tiles/";
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
