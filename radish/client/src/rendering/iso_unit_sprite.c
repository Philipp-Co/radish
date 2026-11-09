#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <radish/rendering/iso_unit_sprite.h>

// Nur die Deklarationen; die Implementierung steht einmal in io/jsmn_impl.c.
#define JSMN_HEADER
#include <jsmn.h>

///
/// Die Tokens einer Datei. Statisch statt auf dem Stapel: 384 Tokens sind gut
/// 6 KB, und der Heap ist tabu (iso_unit_sprite.h).
///
static jsmntok_t rad_iso_unit_sprite_tokens[RAD_ISO_UNIT_SPRITE_TOKENS_MAX];

typedef struct
{
    const char *json;
    const jsmntok_t *tokens;
    int32_t number_of_tokens;
} RAD_IsoUnitSpriteTokens_t;

///
/// Die Blickrichtungen, wie sie in der Datei heissen, in der Reihenfolge von
/// RAD_IsoFacing_t: sw = X_MINUS, se = Y_PLUS, ne = X_PLUS, nw = Y_MINUS.
///
static const char *const RAD_ISO_UNIT_FACING_NAMES[RAD_ISO_FACINGS] = { "sw", "se", "ne", "nw" };

///
/// Die Animationen, wie sie in der Datei heissen, in der Reihenfolge von
/// RAD_IsoUnitAnimation_t.
///
static const char *const RAD_ISO_UNIT_ANIMATION_NAMES[RAD_ISO_UNIT_ANIMATIONS] = {
    "turn", "aim", "move", "fire", "destroyed"
};

/// Wie in control/start_game.c: die Pflichtschluessel eines Objekts als Bits.
#define RAD_FOUND(bit) (1u << (bit))

const char* RAD_IsoUnitSpriteResultText(RAD_IsoUnitSpriteResult_t result)
{
    switch(result)
    {
        case RAD_ISO_UNIT_SPRITE_OK:                      return "in Ordnung";
        case RAD_ISO_UNIT_SPRITE_ERROR_TOO_LARGE:         return "zu gross";
        case RAD_ISO_UNIT_SPRITE_ERROR_SYNTAX:            return "kein gueltiges JSON";
        case RAD_ISO_UNIT_SPRITE_ERROR_TOO_MANY_TOKENS:   return "zu viele Eintraege";
        case RAD_ISO_UNIT_SPRITE_ERROR_SCHEMA:            return "nicht nach sprite.schema.json";
        default:                                          return "unbekanntes Ergebnis";
    }
}

RAD_IsoUnitAtlas_t RAD_IsoUnitAtlasOf(RAD_IsoUnitAnimation_t animation, RAD_IsoFacing_t facing)
{
    if(animation == RAD_ISO_UNIT_ANIMATION_TURN)
    {
        return RAD_ISO_UNIT_ATLAS_TURN;
    }
    if((animation < 0) || (animation >= RAD_ISO_UNIT_ANIMATIONS) || (facing < 0) || (facing >= RAD_ISO_FACINGS))
    {
        return -1;
    }
    return 1 + ((int32_t)animation - 1) * RAD_ISO_FACINGS + (int32_t)facing;
}

bool RAD_IsoUnitAnimationIsRotation(RAD_IsoUnitAnimation_t animation)
{
    return (animation == RAD_ISO_UNIT_ANIMATION_TURN) || (animation == RAD_ISO_UNIT_ANIMATION_AIM);
}

bool RAD_IsoUnitAnimationLoops(RAD_IsoUnitAnimation_t animation)
{
    return animation == RAD_ISO_UNIT_ANIMATION_MOVE;
}

// ---------------------------------------------------------------------------
// Tokens lesen
// ---------------------------------------------------------------------------

static bool RAD_IsoUnitIsType(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, jsmntype_t type)
{
    return (index >= 0) && (index < t->number_of_tokens) && (t->tokens[index].type == type);
}

///
/// Der Index hinter dem Wert an "index" samt allem, was darin verschachtelt ist
/// -- wie RAD_SubtreeEnd in control/start_game.c. Die Tiefe ist die der Datei,
/// und die ist flach (hoechstens vier Ebenen).
///
static int32_t RAD_IsoUnitSubtreeEnd(const RAD_IsoUnitSpriteTokens_t *t, int32_t index)
{
    if((index < 0) || (index >= t->number_of_tokens))
    {
        return t->number_of_tokens;
    }

    int32_t next = index + 1;
    for(int32_t i = 0; i < t->tokens[index].size; ++i)
    {
        next = RAD_IsoUnitSubtreeEnd(t, next);
    }
    return next;
}

static int32_t RAD_IsoUnitTokenLength(const RAD_IsoUnitSpriteTokens_t *t, int32_t index)
{
    return (int32_t)(t->tokens[index].end - t->tokens[index].start);
}

static const char* RAD_IsoUnitTokenText(const RAD_IsoUnitSpriteTokens_t *t, int32_t index)
{
    return t->json + t->tokens[index].start;
}

static bool RAD_IsoUnitKeyIs(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, const char *key)
{
    const size_t length = strlen(key);
    return RAD_IsoUnitIsType(t, index, JSMN_STRING)
           && ((size_t)RAD_IsoUnitTokenLength(t, index) == length)
           && (memcmp(RAD_IsoUnitTokenText(t, index), key, length) == 0);
}

///
/// Welcher Eintrag aus "names" der Schluessel an "index" ist; -1 fuer keinen.
///
static int32_t RAD_IsoUnitKeyIndex(const RAD_IsoUnitSpriteTokens_t *t, int32_t index,
                                   const char *const *names, int32_t number_of_names)
{
    for(int32_t i = 0; i < number_of_names; ++i)
    {
        if(RAD_IsoUnitKeyIs(t, index, names[i]))
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
static bool RAD_IsoUnitReadInt(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, int32_t min, int32_t max, int32_t *out)
{
    if(!RAD_IsoUnitIsType(t, index, JSMN_PRIMITIVE))
    {
        return false;
    }

    const char *text = RAD_IsoUnitTokenText(t, index);
    const int32_t length = RAD_IsoUnitTokenLength(t, index);
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
/// Eine Zahl, auch mit Nachkommastellen. strtod braucht eine Null am Ende; die
/// Zahl wird dafuer in einen kleinen Puffer kopiert -- laengere Zahlen sind hier
/// ohnehin falsch.
///
static bool RAD_IsoUnitReadNumber(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, float *out)
{
    char buffer[32];
    if(!RAD_IsoUnitIsType(t, index, JSMN_PRIMITIVE))
    {
        return false;
    }

    const int32_t length = RAD_IsoUnitTokenLength(t, index);
    const char *text = RAD_IsoUnitTokenText(t, index);
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
    *out = (float)value;
    return true;
}

static bool RAD_IsoUnitReadBool(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, bool *out)
{
    if(!RAD_IsoUnitIsType(t, index, JSMN_PRIMITIVE))
    {
        return false;
    }
    const char *text = RAD_IsoUnitTokenText(t, index);
    const int32_t length = RAD_IsoUnitTokenLength(t, index);
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

static bool RAD_IsoUnitIsKeyCharacter(char c)
{
    return ((c >= 'a') && (c <= 'z')) || ((c >= '0') && (c <= '9')) || (c == '_');
}

///
/// Ein Dateiname nach "^[a-z0-9_]{1,27}\.png$". Weil nur diese Zeichen gehen,
/// gibt es keine Maskierung, die der rohe Ausschnitt verfaelschen koennte.
///
static bool RAD_IsoUnitReadFileName(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, char out[RAD_ISO_UNIT_FILE_NAME_MAX])
{
    static const char extension[] = ".png";
    const int32_t extension_length = (int32_t)(sizeof(extension) - 1);

    if(!RAD_IsoUnitIsType(t, index, JSMN_STRING))
    {
        return false;
    }
    const char *text = RAD_IsoUnitTokenText(t, index);
    const int32_t length = RAD_IsoUnitTokenLength(t, index);
    const int32_t stem = length - extension_length;
    if((stem < 1) || (length >= RAD_ISO_UNIT_FILE_NAME_MAX)
       || (memcmp(text + stem, extension, (size_t)extension_length) != 0))
    {
        return false;
    }
    for(int32_t i = 0; i < stem; ++i)
    {
        if(!RAD_IsoUnitIsKeyCharacter(text[i]))
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

static bool RAD_IsoUnitReadKeyFrames(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, int32_t out[RAD_ISO_FACINGS])
{
    if(!RAD_IsoUnitIsType(t, index, JSMN_ARRAY) || (t->tokens[index].size != RAD_ISO_FACINGS))
    {
        return false;
    }
    // Die Elemente sind Primitive ohne Kinder: sie folgen direkt aufeinander.
    for(int32_t i = 0; i < RAD_ISO_FACINGS; ++i)
    {
        if(!RAD_IsoUnitReadInt(t, index + 1 + i, 0, RAD_ISO_UNIT_FRAMES_MAX - 1, &out[i]))
        {
            return false;
        }
    }
    return true;
}

///
/// Was ein Schema nicht ausdruecken kann: die Schluesselbilder streng
/// aufsteigend und alle im Atlas.
///
static bool RAD_IsoUnitKeyFramesFit(const RAD_IsoUnitAtlasInfo_t *atlas)
{
    for(int32_t i = 0; i < RAD_ISO_FACINGS; ++i)
    {
        const int32_t previous = (i > 0) ? atlas->key_frames[i - 1] : -1;
        if((atlas->key_frames[i] <= previous) || (atlas->key_frames[i] >= atlas->frame_count))
        {
            return false;
        }
    }
    return true;
}

///
/// Ein Atlas ($defs/rotation oder $defs/sequence): die gemeinsamen Felder, dazu
/// key_frames bei einer Drehung, fps bei einer Folge -- das jeweils andere ist
/// ein unbekannter Schluessel.
///
static bool RAD_IsoUnitReadAtlas(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, bool rotation, RAD_IsoUnitAtlasInfo_t *atlas)
{
    if(!RAD_IsoUnitIsType(t, index, JSMN_OBJECT))
    {
        return false;
    }

    enum { FILE_NAME, WIDTH, HEIGHT, COUNT, EXTRA, ALL = (1u << 5) - 1 };
    uint32_t found = 0;
    bool ok = true;

    int32_t key = index + 1;
    for(int32_t i = 0; ok && (i < t->tokens[index].size); ++i)
    {
        const int32_t value = key + 1;
        if(RAD_IsoUnitKeyIs(t, key, "file"))
        {
            ok = RAD_IsoUnitReadFileName(t, value, atlas->file);
            found |= RAD_FOUND(FILE_NAME);
        }
        else if(RAD_IsoUnitKeyIs(t, key, "frame_width"))
        {
            ok = RAD_IsoUnitReadInt(t, value, 1, RAD_ISO_UNIT_FRAME_SIZE_MAX, &atlas->frame_width);
            found |= RAD_FOUND(WIDTH);
        }
        else if(RAD_IsoUnitKeyIs(t, key, "frame_height"))
        {
            ok = RAD_IsoUnitReadInt(t, value, 1, RAD_ISO_UNIT_FRAME_SIZE_MAX, &atlas->frame_height);
            found |= RAD_FOUND(HEIGHT);
        }
        else if(RAD_IsoUnitKeyIs(t, key, "frame_count"))
        {
            ok = RAD_IsoUnitReadInt(t, value, 1, RAD_ISO_UNIT_FRAMES_MAX, &atlas->frame_count);
            found |= RAD_FOUND(COUNT);
        }
        else if(rotation && RAD_IsoUnitKeyIs(t, key, "key_frames"))
        {
            ok = RAD_IsoUnitReadKeyFrames(t, value, atlas->key_frames);
            found |= RAD_FOUND(EXTRA);
        }
        else if(!rotation && RAD_IsoUnitKeyIs(t, key, "fps"))
        {
            ok = RAD_IsoUnitReadNumber(t, value, &atlas->fps) && (atlas->fps > 0.0f) && (atlas->fps <= RAD_ISO_UNIT_FPS_MAX);
            found |= RAD_FOUND(EXTRA);
        }
        else
        {
            ok = false;
        }
        key = RAD_IsoUnitSubtreeEnd(t, key);
    }

    return ok && (found == ALL) && (!rotation || RAD_IsoUnitKeyFramesFit(atlas));
}

///
/// Eine Animation mit einem Atlas je Blickrichtung ($defs/facings): alle vier,
/// keine weitere.
///
static bool RAD_IsoUnitReadFacings(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, RAD_IsoUnitAnimation_t animation,
                                   RAD_IsoUnitSpriteInfo_t *info)
{
    if(!RAD_IsoUnitIsType(t, index, JSMN_OBJECT))
    {
        return false;
    }

    const bool rotation = RAD_IsoUnitAnimationIsRotation(animation);
    uint32_t found = 0;
    bool ok = true;

    int32_t key = index + 1;
    for(int32_t i = 0; ok && (i < t->tokens[index].size); ++i)
    {
        const int32_t facing = RAD_IsoUnitKeyIndex(t, key, RAD_ISO_UNIT_FACING_NAMES, RAD_ISO_FACINGS);
        ok = (facing >= 0)
             && RAD_IsoUnitReadAtlas(t, key + 1, rotation,
                                     &info->atlases[RAD_IsoUnitAtlasOf(animation, (RAD_IsoFacing_t)facing)]);
        if(ok)
        {
            found |= RAD_FOUND(facing);
        }
        key = RAD_IsoUnitSubtreeEnd(t, key);
    }
    return ok && (found == RAD_FOUND(RAD_ISO_FACINGS) - 1);
}

static bool RAD_IsoUnitReadAnimations(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, RAD_IsoUnitSpriteInfo_t *info)
{
    if(!RAD_IsoUnitIsType(t, index, JSMN_OBJECT))
    {
        return false;
    }

    uint32_t found = 0;
    bool ok = true;

    int32_t key = index + 1;
    for(int32_t i = 0; ok && (i < t->tokens[index].size); ++i)
    {
        const int32_t animation = RAD_IsoUnitKeyIndex(t, key, RAD_ISO_UNIT_ANIMATION_NAMES, RAD_ISO_UNIT_ANIMATIONS);
        if(animation < 0)
        {
            ok = false;
        }
        else if(animation == RAD_ISO_UNIT_ANIMATION_TURN)
        {
            ok = RAD_IsoUnitReadAtlas(t, key + 1, true, &info->atlases[RAD_ISO_UNIT_ATLAS_TURN]);
        }
        else
        {
            ok = RAD_IsoUnitReadFacings(t, key + 1, (RAD_IsoUnitAnimation_t)animation, info);
        }
        if(ok)
        {
            found |= RAD_FOUND(animation);
        }
        key = RAD_IsoUnitSubtreeEnd(t, key);
    }
    return ok && (found == RAD_FOUND(RAD_ISO_UNIT_ANIMATIONS) - 1);
}

static bool RAD_IsoUnitReadAnchor(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, RAD_IsoUnitSpriteInfo_t *info)
{
    if(!RAD_IsoUnitIsType(t, index, JSMN_OBJECT))
    {
        return false;
    }

    enum { X, Y, ALL = (1u << 2) - 1 };
    uint32_t found = 0;
    bool ok = true;

    int32_t key = index + 1;
    for(int32_t i = 0; ok && (i < t->tokens[index].size); ++i)
    {
        float *value = NULL;
        if(RAD_IsoUnitKeyIs(t, key, "x"))
        {
            value = &info->anchor_x;
            found |= RAD_FOUND(X);
        }
        else if(RAD_IsoUnitKeyIs(t, key, "y"))
        {
            value = &info->anchor_y;
            found |= RAD_FOUND(Y);
        }
        ok = (value != NULL) && RAD_IsoUnitReadNumber(t, key + 1, value) && (*value >= 0.0f) && (*value <= 1.0f);
        key = RAD_IsoUnitSubtreeEnd(t, key);
    }
    return ok && (found == ALL);
}

///
/// "timing_ms": jeder Wert darf fehlen, keiner ist 0 oder kleiner.
///
static bool RAD_IsoUnitReadTiming(const RAD_IsoUnitSpriteTokens_t *t, int32_t index, RAD_IsoUnitSpriteInfo_t *info)
{
    if(!RAD_IsoUnitIsType(t, index, JSMN_OBJECT))
    {
        return false;
    }

    bool ok = true;
    int32_t key = index + 1;
    for(int32_t i = 0; ok && (i < t->tokens[index].size); ++i)
    {
        float *value = NULL;
        if(RAD_IsoUnitKeyIs(t, key, "step"))
        {
            value = &info->step_ms;
        }
        else if(RAD_IsoUnitKeyIs(t, key, "turn"))
        {
            value = &info->turn_ms;
        }
        else if(RAD_IsoUnitKeyIs(t, key, "aim"))
        {
            value = &info->aim_ms;
        }
        else if(RAD_IsoUnitKeyIs(t, key, "aim_hold"))
        {
            value = &info->aim_hold_ms;
        }
        ok = (value != NULL) && RAD_IsoUnitReadNumber(t, key + 1, value)
             && (*value > 0.0f) && (*value <= RAD_ISO_UNIT_DURATION_MS_MAX);
        key = RAD_IsoUnitSubtreeEnd(t, key);
    }
    return ok;
}

///
/// Die Wurzel: Pflicht sind version, animations, anchor und draw_width.
///
static bool RAD_IsoUnitReadRoot(const RAD_IsoUnitSpriteTokens_t *t, RAD_IsoUnitSpriteInfo_t *info)
{
    if(!RAD_IsoUnitIsType(t, 0, JSMN_OBJECT))
    {
        return false;
    }

    enum { VERSION, ANIMATIONS, ANCHOR, DRAW_WIDTH, ALL = (1u << 4) - 1 };
    uint32_t found = 0;
    bool ok = true;

    int32_t key = 1;
    for(int32_t i = 0; ok && (i < t->tokens[0].size); ++i)
    {
        const int32_t value = key + 1;
        if(RAD_IsoUnitKeyIs(t, key, "version"))
        {
            int32_t version = 0;
            ok = RAD_IsoUnitReadInt(t, value, 1, 1, &version);
            found |= RAD_FOUND(VERSION);
        }
        else if(RAD_IsoUnitKeyIs(t, key, "animations"))
        {
            ok = RAD_IsoUnitReadAnimations(t, value, info);
            found |= RAD_FOUND(ANIMATIONS);
        }
        else if(RAD_IsoUnitKeyIs(t, key, "anchor"))
        {
            ok = RAD_IsoUnitReadAnchor(t, value, info);
            found |= RAD_FOUND(ANCHOR);
        }
        else if(RAD_IsoUnitKeyIs(t, key, "draw_width"))
        {
            ok = RAD_IsoUnitReadInt(t, value, 1, RAD_ISO_UNIT_DRAW_WIDTH_MAX, &info->draw_width);
            found |= RAD_FOUND(DRAW_WIDTH);
        }
        else if(RAD_IsoUnitKeyIs(t, key, "own_shadow"))
        {
            ok = RAD_IsoUnitReadBool(t, value, &info->own_shadow);
        }
        else if(RAD_IsoUnitKeyIs(t, key, "timing_ms"))
        {
            ok = RAD_IsoUnitReadTiming(t, value, info);
        }
        else
        {
            ok = false;
        }
        key = RAD_IsoUnitSubtreeEnd(t, key);
    }
    return ok && (found == ALL);
}

RAD_IsoUnitSpriteResult_t RAD_IsoUnitSpriteParse(const char *json, size_t length, RAD_IsoUnitSpriteInfo_t *out)
{
    if((json == NULL) || (out == NULL))
    {
        return RAD_ISO_UNIT_SPRITE_ERROR_SYNTAX;
    }
    if(length > RAD_ISO_UNIT_SPRITE_JSON_MAX)
    {
        return RAD_ISO_UNIT_SPRITE_ERROR_TOO_LARGE;
    }

    jsmn_parser parser;
    jsmn_init(&parser);
    const int count = jsmn_parse(&parser, json, length, rad_iso_unit_sprite_tokens, RAD_ISO_UNIT_SPRITE_TOKENS_MAX);
    if(count == JSMN_ERROR_NOMEM)
    {
        return RAD_ISO_UNIT_SPRITE_ERROR_TOO_MANY_TOKENS;
    }
    if(count < 1)
    {
        return RAD_ISO_UNIT_SPRITE_ERROR_SYNTAX;
    }

    const RAD_IsoUnitSpriteTokens_t tokens = {
        .json = json,
        .tokens = rad_iso_unit_sprite_tokens,
        .number_of_tokens = count
    };

    // Erst in eine eigene Beschreibung: bei einem Fehler bleibt "out", wie es war.
    RAD_IsoUnitSpriteInfo_t info;
    memset(&info, 0, sizeof(info));
    if(!RAD_IsoUnitReadRoot(&tokens, &info))
    {
        return RAD_ISO_UNIT_SPRITE_ERROR_SCHEMA;
    }
    *out = info;
    return RAD_ISO_UNIT_SPRITE_OK;
}

bool RAD_IsoUnitSpriteKeyValid(const char *key)
{
    if(key == NULL)
    {
        return false;
    }
    size_t length = 0;
    for(; key[length] != '\0'; ++length)
    {
        if((length >= RAD_ISO_UNIT_SPRITE_KEY_MAX - 1) || !RAD_IsoUnitIsKeyCharacter(key[length]))
        {
            return false;
        }
    }
    return length > 0;
}

bool RAD_IsoUnitSpritePath(const char *key, const char *file, char *out, size_t size)
{
    if((out == NULL) || (size == 0))
    {
        return false;
    }
    out[0] = '\0';
    if(!RAD_IsoUnitSpriteKeyValid(key) || (file == NULL) || (file[0] == '\0'))
    {
        return false;
    }

    static const char prefix[] = "/assets/units/";
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

int32_t RAD_IsoUnitSequenceFrame(const RAD_IsoUnitAtlasInfo_t *atlas, float elapsed_ms, bool loop)
{
    if((atlas == NULL) || (atlas->frame_count <= 0) || (atlas->fps <= 0.0f))
    {
        return -1;
    }

    const float elapsed = fmaxf(elapsed_ms, 0.0f);
    // Gross genug fuer jede Spieldauer: bei 60 fps reichen 2^31 Bilder ueber ein Jahr.
    const double frames = floor((double)elapsed * (double)atlas->fps / 1000.0);
    const int64_t frame = (frames > (double)INT32_MAX) ? INT32_MAX : (int64_t)frames;
    if(loop)
    {
        return (int32_t)(frame % atlas->frame_count);
    }
    return (int32_t)((frame < atlas->frame_count) ? frame : atlas->frame_count - 1);
}

float RAD_IsoUnitSequenceMs(const RAD_IsoUnitAtlasInfo_t *atlas)
{
    if((atlas == NULL) || (atlas->fps <= 0.0f))
    {
        return 0.0f;
    }
    return (float)atlas->frame_count * 1000.0f / atlas->fps;
}
