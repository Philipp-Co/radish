#include <SDL2/SDL_image.h>
#include <dirent.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <radish/view/rendering/iso_effect_images.h>
#include <radish/view/rendering/iso_effect_sprite.h>

///
/// Wo die Ordner der Effekte liegen (RAD_IsoEffectSpritePath).
///
#define RAD_ISO_EFFECT_DIRECTORY "/assets/effects"

///
/// Ein geladener Effekt: seine Beschreibung und je Richtung der Atlas samt der
/// Zellen je Zeile -- das sagt erst die Breite der Datei. Richtungen mit
/// derselben Datei zeigen auf dieselbe Textur. "loaded": nur dann gibt es ihn.
///
typedef struct
{
    bool loaded;
    char key[RAD_ISO_EFFECT_KEY_MAX];
    RAD_IsoEffectSpriteInfo_t info;
    SDL_Texture *atlases[RAD_ISO_FACINGS];
    int32_t columns[RAD_ISO_FACINGS];
} RAD_IsoEffectSlot_t;

static RAD_IsoEffectSlot_t rad_iso_effect_slots[RAD_ISO_EFFECT_SPRITES];

///
/// Der Lesepuffer fuer eine effect.json, eins groesser als erlaubt: so faellt
/// eine zu grosse Datei auf, ohne dass sie ganz gelesen werden muss.
///
static char rad_iso_effect_json[RAD_ISO_EFFECT_SPRITE_JSON_MAX + 1];

static void RAD_IsoEffectReport(const char *key, const char *reason, const char *detail)
{
    printf("Effekt \"%s\": %s%s%s\n", key, reason, (detail != NULL) ? " " : "", (detail != NULL) ? detail : "");
}

static const RAD_IsoEffectSlot_t* RAD_IsoEffectSlot(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing)
{
    if((sprite < 0) || (sprite >= RAD_ISO_EFFECT_SPRITES) || (facing < 0) || (facing >= RAD_ISO_FACINGS)
       || !rad_iso_effect_slots[sprite].loaded)
    {
        return NULL;
    }
    return &rad_iso_effect_slots[sprite];
}

RAD_IsoEffectSprite_t RAD_IsoEffectRandomExplosion(void)
{
    RAD_IsoEffectSprite_t explosions[RAD_ISO_EFFECT_SPRITES];
    int32_t count = 0;
    for(int32_t sprite = 0; sprite < RAD_ISO_EFFECT_SPRITES; ++sprite)
    {
        const RAD_IsoEffectSlot_t *slot = &rad_iso_effect_slots[sprite];
        if(slot->loaded && (slot->info.kind == RAD_ISO_EFFECT_KIND_EXPLOSION))
        {
            explosions[count++] = sprite;
        }
    }
    return (count > 0) ? explosions[(size_t)rand() % (size_t)count] : RAD_ISO_EFFECT_SPRITE_NONE;
}

// ---------------------------------------------------------------------------
// Laden
// ---------------------------------------------------------------------------

static int RAD_IsoEffectCompareKeys(const void *a, const void *b)
{
    return strcmp((const char *)a, (const char *)b);
}

///
/// Die Ordner unter RAD_ISO_EFFECT_DIRECTORY nach "keys", sortiert; ihre Zahl.
/// Was kein gueltiger Schluessel ist (".", "..", fremde Namen), zaehlt nicht.
/// Fehlt das Verzeichnis, gibt es keine Effekte.
///
static int32_t RAD_IsoEffectListKeys(char keys[RAD_ISO_EFFECT_SPRITES][RAD_ISO_EFFECT_KEY_MAX])
{
    DIR *directory = opendir(RAD_ISO_EFFECT_DIRECTORY);
    if(directory == NULL)
    {
        printf("Keine Effekte: %s fehlt\n", RAD_ISO_EFFECT_DIRECTORY);
        return 0;
    }

    int32_t count = 0;
    const struct dirent *entry = NULL;
    while((entry = readdir(directory)) != NULL)
    {
        if(!RAD_IsoEffectSpriteKeyValid(entry->d_name))
        {
            continue;
        }
        if(count >= RAD_ISO_EFFECT_SPRITES)
        {
            RAD_IsoEffectReport(entry->d_name, "bleibt liegen: zu viele Effekte", NULL);
            continue;
        }
        strcpy(keys[count++], entry->d_name);
    }
    closedir(directory);

    qsort(keys, (size_t)count, RAD_ISO_EFFECT_KEY_MAX, RAD_IsoEffectCompareKeys);
    return count;
}

///
/// Liest die effect.json von "key" nach "info". false mit einer Meldung, wenn
/// sie fehlt, zu gross oder ungueltig ist.
///
static bool RAD_IsoEffectReadInfo(const char *key, RAD_IsoEffectSpriteInfo_t *info)
{
    char path[RAD_ISO_EFFECT_PATH_MAX];
    if(!RAD_IsoEffectSpritePath(key, "effect.json", path, sizeof(path)))
    {
        RAD_IsoEffectReport(key, "ist kein gueltiger Ordner", NULL);
        return false;
    }

    SDL_RWops *file = SDL_RWFromFile(path, "rb");
    if(file == NULL)
    {
        RAD_IsoEffectReport(key, "hat keine effect.json", NULL);
        return false;
    }
    const size_t length = SDL_RWread(file, rad_iso_effect_json, 1, sizeof(rad_iso_effect_json));
    SDL_RWclose(file);

    const RAD_IsoEffectSpriteResult_t result = RAD_IsoEffectSpriteParse(rad_iso_effect_json, length, info);
    if(result != RAD_ISO_EFFECT_SPRITE_OK)
    {
        RAD_IsoEffectReport(key, "effect.json ist", RAD_IsoEffectSpriteResultText(result));
        return false;
    }
    return true;
}

///
/// Wie viele Zellen in einer Zeile des Atlas "texture" fuer "atlas" liegen; 0,
/// wenn der Atlas zu klein fuer die Folge ist.
///
static int32_t RAD_IsoEffectAtlasColumns(const char *key, const RAD_IsoEffectAtlasInfo_t *atlas, SDL_Texture *texture)
{
    int32_t w = 0, h = 0;
    SDL_QueryTexture(texture, NULL, NULL, &w, &h);

    const int32_t columns = w / atlas->frame_width;
    const int32_t rows = h / atlas->frame_height;
    if(columns * rows < atlas->first_frame + atlas->frame_count)
    {
        printf("Effekt \"%s\": Atlas %s hat %d x %d Zellen, %d gebraucht\n", key, atlas->file, (int)columns,
               (int)rows, (int)(atlas->first_frame + atlas->frame_count));
        return 0;
    }
    return columns;
}

///
/// Die schon geladene Textur der Datei "file" in "slot" vor Richtung "before",
/// oder NULL.
///
static SDL_Texture* RAD_IsoEffectLoadedAtlas(const RAD_IsoEffectSlot_t *slot, int32_t before, const char *file)
{
    for(int32_t facing = 0; facing < before; ++facing)
    {
        if((slot->atlases[facing] != NULL) && (strcmp(slot->info.atlases[facing].file, file) == 0))
        {
            return slot->atlases[facing];
        }
    }
    return NULL;
}

static void RAD_IsoEffectFreeSlot(RAD_IsoEffectSlot_t *slot)
{
    for(int32_t facing = 0; facing < RAD_ISO_FACINGS; ++facing)
    {
        SDL_Texture *texture = slot->atlases[facing];
        if(texture == NULL)
        {
            continue;
        }
        // Geteilte Atlanten nur einmal freigeben.
        SDL_DestroyTexture(texture);
        for(int32_t other = facing; other < RAD_ISO_FACINGS; ++other)
        {
            if(slot->atlases[other] == texture)
            {
                slot->atlases[other] = NULL;
            }
        }
    }
    memset(slot, 0, sizeof(*slot));
}

///
/// Laedt die Atlanten von "slot" je Richtung. false mit Meldung, wenn einer
/// fehlt oder nicht passt -- dann ist nichts davon mehr geladen.
///
static bool RAD_IsoEffectLoadAtlases(SDL_Renderer *renderer, RAD_IsoEffectSlot_t *slot)
{
    for(int32_t facing = 0; facing < RAD_ISO_FACINGS; ++facing)
    {
        const RAD_IsoEffectAtlasInfo_t *atlas = &slot->info.atlases[facing];
        SDL_Texture *texture = RAD_IsoEffectLoadedAtlas(slot, facing, atlas->file);
        if(texture == NULL)
        {
            char path[RAD_ISO_EFFECT_PATH_MAX];
            if(RAD_IsoEffectSpritePath(slot->key, atlas->file, path, sizeof(path)))
            {
                texture = IMG_LoadTexture(renderer, path);
            }
            if(texture == NULL)
            {
                RAD_IsoEffectReport(slot->key, IMG_GetError(), atlas->file);
                return false;
            }
            // Wie bei den Einheiten: vergroessert gezeichnet, ohne das kantig.
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
        }
        // Gleich eintragen: so gibt RAD_IsoEffectFreeSlot sie auch im Fehlerfall frei.
        slot->atlases[facing] = texture;

        slot->columns[facing] = RAD_IsoEffectAtlasColumns(slot->key, atlas, texture);
        if(slot->columns[facing] == 0)
        {
            return false;
        }
    }
    return true;
}

static bool RAD_IsoEffectLoadSlot(SDL_Renderer *renderer, const char *key, RAD_IsoEffectSlot_t *slot)
{
    strcpy(slot->key, key);
    if(!RAD_IsoEffectReadInfo(key, &slot->info) || !RAD_IsoEffectLoadAtlases(renderer, slot))
    {
        RAD_IsoEffectFreeSlot(slot);
        return false;
    }
    slot->loaded = true;
    return true;
}

bool RAD_IsoEffectImagesLoad(SDL_Renderer *renderer)
{
    RAD_IsoEffectImagesFree();

    static char keys[RAD_ISO_EFFECT_SPRITES][RAD_ISO_EFFECT_KEY_MAX];
    const int32_t count = RAD_IsoEffectListKeys(keys);

    bool complete = true;
    for(int32_t sprite = 0; sprite < count; ++sprite)
    {
        complete = RAD_IsoEffectLoadSlot(renderer, keys[sprite], &rad_iso_effect_slots[sprite]) && complete;
    }
    return complete;
}

void RAD_IsoEffectImagesFree(void)
{
    for(int32_t sprite = 0; sprite < RAD_ISO_EFFECT_SPRITES; ++sprite)
    {
        RAD_IsoEffectFreeSlot(&rad_iso_effect_slots[sprite]);
    }
}

// ---------------------------------------------------------------------------
// Abfragen
// ---------------------------------------------------------------------------

float RAD_IsoEffectDurationMs(RAD_IsoEffectSprite_t sprite)
{
    const RAD_IsoEffectSlot_t *slot = RAD_IsoEffectSlot(sprite, 0);
    return (slot != NULL) ? slot->info.duration_ms : 0.0f;
}

SDL_Texture* RAD_IsoEffectAtlasTexture(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing)
{
    const RAD_IsoEffectSlot_t *slot = RAD_IsoEffectSlot(sprite, facing);
    return (slot != NULL) ? slot->atlases[facing] : NULL;
}

int32_t RAD_IsoEffectFrameAt(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing, float progress)
{
    const RAD_IsoEffectSlot_t *slot = RAD_IsoEffectSlot(sprite, facing);
    if(slot == NULL)
    {
        return -1;
    }

    const int32_t count = slot->info.atlases[facing].frame_count;
    const float clamped = fminf(fmaxf(progress, 0.0f), 1.0f);
    const int32_t frame = (int32_t)(clamped * (float)count);
    return (frame < count) ? frame : (count - 1);
}

bool RAD_IsoEffectFrameSource(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing, int32_t frame, SDL_Rect *source)
{
    const RAD_IsoEffectSlot_t *slot = RAD_IsoEffectSlot(sprite, facing);
    if(slot == NULL)
    {
        return false;
    }

    const RAD_IsoEffectAtlasInfo_t *atlas = &slot->info.atlases[facing];
    if((frame < 0) || (frame >= atlas->frame_count))
    {
        return false;
    }

    const int32_t columns = slot->columns[facing];
    const int32_t cell = atlas->first_frame + frame;
    *source = (SDL_Rect){
        .x = (cell % columns) * atlas->frame_width,
        .y = (cell / columns) * atlas->frame_height,
        .w = atlas->frame_width,
        .h = atlas->frame_height
    };
    return true;
}

bool RAD_IsoEffectFrameTarget(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing, int32_t x, int32_t y,
                              SDL_Rect *target)
{
    const RAD_IsoEffectSlot_t *slot = RAD_IsoEffectSlot(sprite, facing);
    if(slot == NULL)
    {
        return false;
    }

    const RAD_IsoEffectAtlasInfo_t *atlas = &slot->info.atlases[facing];
    const int32_t w = slot->info.draw_width;
    const int32_t h = (int32_t)lroundf((float)w * (float)atlas->frame_height / (float)atlas->frame_width);
    *target = (SDL_Rect){
        .x = x - (int32_t)lroundf(atlas->anchor_x * (float)w),
        .y = y - (int32_t)lroundf(atlas->anchor_y * (float)h),
        .w = w,
        .h = h
    };
    return true;
}
