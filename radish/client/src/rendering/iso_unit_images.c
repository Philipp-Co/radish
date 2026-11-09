#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <radish/model/unit.h>
#include <radish/rendering/iso_unit_images.h>

_Static_assert(RAD_ISO_UNIT_SPRITE_KEY_MAX == RAD_CLIENT_UNIT_SPRITE_KEY_MAX,
               "Bild-Schluessel in Modell und Bildern gleich gross");

typedef enum
{
    RAD_ISO_UNIT_SLOT_FREE = 0,
    RAD_ISO_UNIT_SLOT_LOADED,
    /// Nicht zu laden -- bleibt ein Rechteck, bis RAD_IsoUnitImagesFree.
    RAD_ISO_UNIT_SLOT_FAILED
} RAD_IsoUnitSlotState_t;

///
/// Ein Platz fuer ein Bild: sein Schluessel, seine Beschreibung und die
/// Texturen. Mehrere Atlanten duerfen dieselbe Datei nennen; dann zeigen ihre
/// Eintraege in "textures" auf dieselbe Textur, und
/// RAD_IsoUnitSlotReleaseTextures gibt sie nur einmal frei.
///
typedef struct
{
    RAD_IsoUnitSlotState_t state;
    char key[RAD_ISO_UNIT_SPRITE_KEY_MAX];
    RAD_IsoUnitSpriteInfo_t info;
    SDL_Texture *textures[RAD_ISO_UNIT_ATLASES];

    ///
    /// Wie viele Zellen in einer Zeile liegen -- das sagt erst die Breite der
    /// Datei, und sie haengt an der Zellbreite des Atlas, nicht an der Datei.
    ///
    int32_t columns[RAD_ISO_UNIT_ATLASES];
} RAD_IsoUnitSlot_t;

static RAD_IsoUnitSlot_t rad_iso_unit_slots[RAD_ISO_UNIT_SPRITES_MAX];
static int32_t rad_iso_unit_number_of_slots = 0;
static SDL_Renderer *rad_iso_unit_renderer = NULL;

/// Ob schon gemeldet ist, dass kein Platz mehr frei war -- einmal reicht.
static bool rad_iso_unit_full_reported = false;

///
/// Der Lesepuffer fuer eine sprite.json, eins groesser als erlaubt: so faellt
/// eine zu grosse Datei auf, ohne dass sie ganz gelesen werden muss.
///
static char rad_iso_unit_json[RAD_ISO_UNIT_SPRITE_JSON_MAX + 1];

static const RAD_IsoUnitSlot_t* RAD_IsoUnitLoadedSlot(RAD_IsoUnitSprite_t sprite)
{
    if((sprite < 0) || (sprite >= rad_iso_unit_number_of_slots)
       || (rad_iso_unit_slots[sprite].state != RAD_ISO_UNIT_SLOT_LOADED))
    {
        return NULL;
    }
    return &rad_iso_unit_slots[sprite];
}

static bool RAD_IsoUnitAtlasValid(RAD_IsoUnitAtlas_t atlas)
{
    return (atlas >= 0) && (atlas < RAD_ISO_UNIT_ATLASES);
}

// ---------------------------------------------------------------------------
// Laden und Freigeben
// ---------------------------------------------------------------------------

///
/// Gibt die Texturen von "slot" frei; Schluessel, Zustand und Beschreibung
/// bleiben.
///
static void RAD_IsoUnitSlotReleaseTextures(RAD_IsoUnitSlot_t *slot)
{
    for(int32_t atlas = 0; atlas < RAD_ISO_UNIT_ATLASES; ++atlas)
    {
        SDL_Texture *texture = slot->textures[atlas];
        if(texture == NULL)
        {
            continue;
        }
        // Jede Textur einmal: alle Eintraege, die auf sie zeigen, vergessen sie.
        for(int32_t other = atlas; other < RAD_ISO_UNIT_ATLASES; ++other)
        {
            if(slot->textures[other] == texture)
            {
                slot->textures[other] = NULL;
            }
        }
        SDL_DestroyTexture(texture);
    }
    memset(slot->columns, 0, sizeof(slot->columns));
}

static void RAD_IsoUnitReport(const char *key, const char *reason, const char *detail)
{
    printf("Bild \"%s\": %s%s%s -- die Einheit wird als Rechteck gezeichnet\n",
           key, reason, (detail != NULL) ? " " : "", (detail != NULL) ? detail : "");
}

///
/// Liest die sprite.json von "key" in rad_iso_unit_json und deren Beschreibung
/// nach "info". false mit einer Meldung, wenn sie fehlt, zu gross oder ungueltig
/// ist.
///
static bool RAD_IsoUnitReadInfo(const char *key, RAD_IsoUnitSpriteInfo_t *info)
{
    char path[RAD_ISO_UNIT_PATH_MAX];
    if(!RAD_IsoUnitSpritePath(key, "sprite.json", path, sizeof(path)))
    {
        RAD_IsoUnitReport(key, "ist kein gueltiger Schluessel", NULL);
        return false;
    }

    SDL_RWops *file = SDL_RWFromFile(path, "rb");
    if(file == NULL)
    {
        RAD_IsoUnitReport(key, "nicht gefunden:", path);
        return false;
    }
    const size_t length = SDL_RWread(file, rad_iso_unit_json, 1, sizeof(rad_iso_unit_json));
    SDL_RWclose(file);

    const RAD_IsoUnitSpriteResult_t result = RAD_IsoUnitSpriteParse(rad_iso_unit_json, length, info);
    if(result != RAD_ISO_UNIT_SPRITE_OK)
    {
        RAD_IsoUnitReport(key, "sprite.json ist", RAD_IsoUnitSpriteResultText(result));
        return false;
    }
    return true;
}

///
/// Wie viele Zellen in einer Zeile von "texture" fuer "info" liegen; 0, wenn der
/// Atlas zu klein fuer alle Zellen ist. Die uebrigen Angaben hat schon der Leser
/// geprueft (rendering/iso_unit_sprite.c).
///
static int32_t RAD_IsoUnitAtlasColumns(const RAD_IsoUnitAtlasInfo_t *info, SDL_Texture *texture)
{
    int32_t w = 0, h = 0;
    SDL_QueryTexture(texture, NULL, NULL, &w, &h);

    const int32_t columns = w / info->frame_width;
    const int32_t rows = h / info->frame_height;
    return (columns * rows >= info->frame_count) ? columns : 0;
}

///
/// Laedt alle Atlanten von "slot". Eine Datei, die ein frueherer Atlas schon
/// geladen hat, wird geteilt.
///
static bool RAD_IsoUnitLoadTextures(RAD_IsoUnitSlot_t *slot)
{
    for(int32_t atlas = 0; atlas < RAD_ISO_UNIT_ATLASES; ++atlas)
    {
        const RAD_IsoUnitAtlasInfo_t *info = &slot->info.atlases[atlas];

        SDL_Texture *texture = NULL;
        for(int32_t earlier = 0; (texture == NULL) && (earlier < atlas); ++earlier)
        {
            if(strcmp(slot->info.atlases[earlier].file, info->file) == 0)
            {
                texture = slot->textures[earlier];
            }
        }

        if(texture == NULL)
        {
            char path[RAD_ISO_UNIT_PATH_MAX];
            if(!RAD_IsoUnitSpritePath(slot->key, info->file, path, sizeof(path)))
            {
                RAD_IsoUnitReport(slot->key, "Pfad zu lang fuer", info->file);
                return false;
            }
            texture = IMG_LoadTexture(rad_iso_unit_renderer, path);
            if(texture == NULL)
            {
                RAD_IsoUnitReport(slot->key, IMG_GetError(), path);
                return false;
            }
            // Die Zellen werden vergroessert oder verkleinert gezeichnet; ohne das
            // nimmt SDL das naechste Pixel, und die Figur wird kantig.
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
        }
        // Gleich eintragen: auch eine, die gleich nicht passt, gibt
        // RAD_IsoUnitSlotReleaseTextures so wieder frei.
        slot->textures[atlas] = texture;

        slot->columns[atlas] = RAD_IsoUnitAtlasColumns(info, texture);
        if(slot->columns[atlas] == 0)
        {
            RAD_IsoUnitReport(slot->key, "zu wenige Zellen fuer", info->file);
            return false;
        }
    }
    return true;
}

///
/// Laedt das Bild "key" in den naechsten freien Platz. Gelingt es nicht, bleibt
/// der Platz als FAILED belegt, damit es nicht bei jeder Figur neu versucht wird.
///
static RAD_IsoUnitSprite_t RAD_IsoUnitLoad(const char *key)
{
    if(rad_iso_unit_number_of_slots >= RAD_ISO_UNIT_SPRITES_MAX)
    {
        if(!rad_iso_unit_full_reported)
        {
            printf("Mehr als %d Einheitenbilder -- \"%s\" und alle weiteren werden als Rechteck gezeichnet\n",
                   RAD_ISO_UNIT_SPRITES_MAX, key);
            rad_iso_unit_full_reported = true;
        }
        return RAD_ISO_UNIT_SPRITE_NONE;
    }

    const RAD_IsoUnitSprite_t sprite = rad_iso_unit_number_of_slots++;
    RAD_IsoUnitSlot_t *slot = &rad_iso_unit_slots[sprite];
    memset(slot, 0, sizeof(*slot));
    // Passt: RAD_IsoUnitSpriteOf hat ihn mit RAD_IsoUnitSpriteKeyValid geprueft.
    strncpy(slot->key, key, sizeof(slot->key) - 1);

    if(!RAD_IsoUnitReadInfo(slot->key, &slot->info) || !RAD_IsoUnitLoadTextures(slot))
    {
        RAD_IsoUnitSlotReleaseTextures(slot);
        slot->state = RAD_ISO_UNIT_SLOT_FAILED;
        return RAD_ISO_UNIT_SPRITE_NONE;
    }

    slot->state = RAD_ISO_UNIT_SLOT_LOADED;
    return sprite;
}

void RAD_IsoUnitImagesInit(SDL_Renderer *renderer)
{
    rad_iso_unit_renderer = renderer;
}

void RAD_IsoUnitImagesFree(void)
{
    for(int32_t sprite = 0; sprite < rad_iso_unit_number_of_slots; ++sprite)
    {
        RAD_IsoUnitSlotReleaseTextures(&rad_iso_unit_slots[sprite]);
        memset(&rad_iso_unit_slots[sprite], 0, sizeof(rad_iso_unit_slots[sprite]));
    }
    rad_iso_unit_number_of_slots = 0;
    rad_iso_unit_full_reported = false;
}

RAD_IsoUnitSprite_t RAD_IsoUnitSpriteOf(const char *key)
{
    if((key == NULL) || (key[0] == '\0') || (rad_iso_unit_renderer == NULL))
    {
        return RAD_ISO_UNIT_SPRITE_NONE;
    }

    // Ein ungueltiger Schluessel belegt keinen Platz: er kommt nur aus einem
    // Server, der sich nicht an das Protokoll haelt, und koennte sonst alle Plaetze
    // fuellen. Gemeldet wird er dafuer bei jedem Versuch.
    if(!RAD_IsoUnitSpriteKeyValid(key))
    {
        RAD_IsoUnitReport(key, "ist kein gueltiger Schluessel", NULL);
        return RAD_ISO_UNIT_SPRITE_NONE;
    }

    for(int32_t sprite = 0; sprite < rad_iso_unit_number_of_slots; ++sprite)
    {
        if(strcmp(rad_iso_unit_slots[sprite].key, key) == 0)
        {
            return (rad_iso_unit_slots[sprite].state == RAD_ISO_UNIT_SLOT_LOADED) ? sprite : RAD_ISO_UNIT_SPRITE_NONE;
        }
    }
    return RAD_IsoUnitLoad(key);
}

// ---------------------------------------------------------------------------
// Zeiten
// ---------------------------------------------------------------------------

///
/// Die Beschreibung von "sprite", oder eine leere fuer ein Bild, das nicht
/// geladen ist -- deren Zeiten sind 0, es gilt also der Standard.
///
static const RAD_IsoUnitSpriteInfo_t* RAD_IsoUnitInfoOrEmpty(RAD_IsoUnitSprite_t sprite)
{
    static const RAD_IsoUnitSpriteInfo_t empty = { 0 };
    const RAD_IsoUnitSlot_t *slot = RAD_IsoUnitLoadedSlot(sprite);
    return (slot != NULL) ? &slot->info : &empty;
}

///
/// "value" aus der sprite.json, oder "fallback", wenn sie keinen nennt (0).
///
static float RAD_IsoUnitDuration(float value, float fallback)
{
    return (value > 0.0f) ? value : fallback;
}

float RAD_IsoUnitStepMs(RAD_IsoUnitSprite_t sprite)
{
    return RAD_IsoUnitDuration(RAD_IsoUnitInfoOrEmpty(sprite)->step_ms, RAD_ISO_UNIT_STEP_MS_DEFAULT);
}

float RAD_IsoUnitTurnMs(RAD_IsoUnitSprite_t sprite)
{
    return RAD_IsoUnitDuration(RAD_IsoUnitInfoOrEmpty(sprite)->turn_ms, RAD_ISO_UNIT_TURN_MS_DEFAULT);
}

float RAD_IsoUnitAimMs(RAD_IsoUnitSprite_t sprite)
{
    return RAD_IsoUnitDuration(RAD_IsoUnitInfoOrEmpty(sprite)->aim_ms, RAD_ISO_UNIT_AIM_MS_DEFAULT);
}

float RAD_IsoUnitAimHoldMs(RAD_IsoUnitSprite_t sprite)
{
    return RAD_IsoUnitDuration(RAD_IsoUnitInfoOrEmpty(sprite)->aim_hold_ms, RAD_ISO_UNIT_AIM_HOLD_MS_DEFAULT);
}

// ---------------------------------------------------------------------------
// Einzelbilder
// ---------------------------------------------------------------------------

RAD_IsoUnitAtlas_t RAD_IsoUnitAimAtlas(RAD_IsoFacing_t facing)
{
    return RAD_IsoUnitAtlasOf(RAD_ISO_UNIT_ANIMATION_AIM, facing);
}

SDL_Texture* RAD_IsoUnitAtlasTexture(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAtlas_t atlas)
{
    const RAD_IsoUnitSlot_t *slot = RAD_IsoUnitLoadedSlot(sprite);
    return ((slot != NULL) && RAD_IsoUnitAtlasValid(atlas)) ? slot->textures[atlas] : NULL;
}

bool RAD_IsoUnitCanAim(RAD_IsoUnitSprite_t sprite)
{
    return RAD_IsoUnitLoadedSlot(sprite) != NULL;
}

///
/// Die Zelle von "info", die die Drehung bei "angle" Grad zeigt: zwischen den
/// beiden festen Zellen davor und danach linear, nach der bei 270 Grad weiter
/// bis zum Ende des Atlas, der wieder auf die erste fuehrt.
///
static int32_t RAD_IsoUnitAtlasFrameAt(const RAD_IsoUnitAtlasInfo_t *info, float angle)
{
    const float normalized = RAD_IsoAngleNormalize(angle);
    int32_t quarter = (int32_t)(normalized / 90.0f);
    if(quarter >= RAD_ISO_FACINGS)
    {
        quarter = RAD_ISO_FACINGS - 1;
    }

    const float within = (normalized - 90.0f * (float)quarter) / 90.0f;
    const int32_t from = info->key_frames[quarter];
    const int32_t to = (quarter + 1 < RAD_ISO_FACINGS) ? info->key_frames[quarter + 1] : info->frame_count;
    const int32_t frame = from + (int32_t)lroundf(within * (float)(to - from));
    return frame % info->frame_count;
}

int32_t RAD_IsoUnitFacingFrame(RAD_IsoUnitSprite_t sprite, RAD_IsoFacing_t facing)
{
    const RAD_IsoUnitSlot_t *slot = RAD_IsoUnitLoadedSlot(sprite);
    if((slot == NULL) || (facing < 0) || (facing >= RAD_ISO_FACINGS))
    {
        return -1;
    }
    return slot->info.atlases[RAD_ISO_UNIT_ATLAS_TURN].key_frames[facing];
}

int32_t RAD_IsoUnitTurnFrame(RAD_IsoUnitSprite_t sprite, RAD_IsoFacing_t from, RAD_IsoFacing_t to, float progress)
{
    if((RAD_IsoUnitFacingFrame(sprite, from) < 0) || (RAD_IsoUnitFacingFrame(sprite, to) < 0))
    {
        return -1;
    }

    const float clamped = fminf(fmaxf(progress, 0.0f), 1.0f);
    const int32_t turns = RAD_IsoFacingQuarterTurns(from, to);
    const float angle = RAD_IsoFacingAngle(from) + clamped * 90.0f * (float)turns;
    return RAD_IsoUnitAtlasFrameAt(&RAD_IsoUnitLoadedSlot(sprite)->info.atlases[RAD_ISO_UNIT_ATLAS_TURN], angle);
}

int32_t RAD_IsoUnitAimFrame(RAD_IsoUnitSprite_t sprite, RAD_IsoFacing_t facing, float angle)
{
    const RAD_IsoUnitSlot_t *slot = RAD_IsoUnitLoadedSlot(sprite);
    const RAD_IsoUnitAtlas_t atlas = RAD_IsoUnitAimAtlas(facing);
    if((slot == NULL) || !RAD_IsoUnitAtlasValid(atlas))
    {
        return -1;
    }
    return RAD_IsoUnitAtlasFrameAt(&slot->info.atlases[atlas], angle);
}

int32_t RAD_IsoUnitSequenceFrameOf(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAnimation_t animation, RAD_IsoFacing_t facing,
                                   float elapsed_ms, RAD_IsoUnitAtlas_t *atlas)
{
    const RAD_IsoUnitSlot_t *slot = RAD_IsoUnitLoadedSlot(sprite);
    const RAD_IsoUnitAtlas_t index = RAD_IsoUnitAtlasOf(animation, facing);
    if((slot == NULL) || !RAD_IsoUnitAtlasValid(index) || RAD_IsoUnitAnimationIsRotation(animation))
    {
        return -1;
    }
    if(atlas != NULL)
    {
        *atlas = index;
    }
    return RAD_IsoUnitSequenceFrame(&slot->info.atlases[index], elapsed_ms, RAD_IsoUnitAnimationLoops(animation));
}

float RAD_IsoUnitSequenceMsOf(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAnimation_t animation, RAD_IsoFacing_t facing)
{
    const RAD_IsoUnitSlot_t *slot = RAD_IsoUnitLoadedSlot(sprite);
    const RAD_IsoUnitAtlas_t index = RAD_IsoUnitAtlasOf(animation, facing);
    if((slot == NULL) || !RAD_IsoUnitAtlasValid(index) || RAD_IsoUnitAnimationIsRotation(animation))
    {
        return 0.0f;
    }
    return RAD_IsoUnitSequenceMs(&slot->info.atlases[index]);
}

bool RAD_IsoUnitFrameSource(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAtlas_t atlas, int32_t frame, SDL_Rect *source)
{
    if(RAD_IsoUnitAtlasTexture(sprite, atlas) == NULL)
    {
        return false;
    }

    const RAD_IsoUnitSlot_t *slot = RAD_IsoUnitLoadedSlot(sprite);
    const RAD_IsoUnitAtlasInfo_t *info = &slot->info.atlases[atlas];
    if((frame < 0) || (frame >= info->frame_count))
    {
        return false;
    }

    const int32_t columns = slot->columns[atlas];
    *source = (SDL_Rect){
        .x = (frame % columns) * info->frame_width,
        .y = (frame / columns) * info->frame_height,
        .w = info->frame_width,
        .h = info->frame_height
    };
    return true;
}

bool RAD_IsoUnitFrameTarget(RAD_IsoUnitSprite_t sprite, int32_t x, int32_t y, SDL_Rect *target)
{
    const RAD_IsoUnitSlot_t *slot = RAD_IsoUnitLoadedSlot(sprite);
    if(slot == NULL)
    {
        return false;
    }

    const RAD_IsoUnitSpriteInfo_t *info = &slot->info;
    const RAD_IsoUnitAtlasInfo_t *turn = &info->atlases[RAD_ISO_UNIT_ATLAS_TURN];
    const int32_t w = info->draw_width;
    // frame_width ist nach dem Leser mindestens 1.
    const int32_t h = (int32_t)lroundf((float)w * (float)turn->frame_height / (float)turn->frame_width);
    *target = (SDL_Rect){
        .x = x - (int32_t)lroundf(info->anchor_x * (float)w),
        .y = y - (int32_t)lroundf(info->anchor_y * (float)h),
        .w = w,
        .h = h
    };
    return true;
}

bool RAD_IsoUnitHasOwnShadow(RAD_IsoUnitSprite_t sprite)
{
    const RAD_IsoUnitSlot_t *slot = RAD_IsoUnitLoadedSlot(sprite);
    return (slot != NULL) && slot->info.own_shadow;
}
