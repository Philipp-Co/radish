#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <string.h>
#include <radish/view/rendering/iso_definitions.h>
#include <radish/view/rendering/iso_tile_images.h>
#include <radish/view/rendering/iso_tile_sprite.h>

_Static_assert(RAD_ISO_TILE_DECOR_MAX == RAD_ISO_DECOR_PER_TILE,
               "tile.json nennt so viele Dekorationen, wie ein Feld tragen kann");
_Static_assert(RAD_ISO_TILE_DECOR_OFFSET_MAX == RAD_ISO_TILE_HEIGHT,
               "Versatz einer Dekoration: hoechstens eine Feldhoehe (tile.schema.json)");
_Static_assert(RAD_ISO_TILE_DECOR_OFFSET_MAX <= RAD_ISO_DECOR_MAX_RISE,
               "keine Dekoration ragt hoeher, als die Kamera erlaubt");

///
/// Wie viele Typen es gibt: HIGH_GRASS ist der letzte (model/tile.h).
///
#define RAD_ISO_TILE_TYPES (RAD_CLIENT_TILE_TYPE_HIGH_GRASS + 1)

///
/// Der Ordner jedes Typs unter assets/tiles/ -- sein Name im Spielstart und im
/// Backend (TerrainCode). UNKNOWN hat keinen: ein Typ, den der Client nicht
/// kennt, soll auffallen (view/rendering/iso_tile/unknown.c).
///
static const char *const RAD_ISO_TILE_KEYS[RAD_ISO_TILE_TYPES] = {
    [RAD_CLIENT_TILE_TYPE_UNKNOWN]       = NULL,
    [RAD_CLIENT_TILE_TYPE_GROUND]        = "ground",
    [RAD_CLIENT_TILE_TYPE_WATER_SHALLOW] = "water_shallow",
    [RAD_CLIENT_TILE_TYPE_VOID]          = "void",
    [RAD_CLIENT_TILE_TYPE_MUD]           = "mud",
    [RAD_CLIENT_TILE_TYPE_MEADOW]        = "meadow",
    [RAD_CLIENT_TILE_TYPE_FOREST]        = "forest",
    [RAD_CLIENT_TILE_TYPE_WATER_DEEP]    = "water_deep",
    [RAD_CLIENT_TILE_TYPE_CITY]          = "city",
    [RAD_CLIENT_TILE_TYPE_ROAD]          = "road",
    [RAD_CLIENT_TILE_TYPE_FORTIFICATION] = "fortification",
    [RAD_CLIENT_TILE_TYPE_DRAGONS_TEETH] = "dragons_teeth",
    [RAD_CLIENT_TILE_TYPE_HIGH_GRASS]    = "high_grass"
};

///
/// Was zu einem Typ geladen ist: seine Beschreibung und je Bild darin eine
/// Textur, NULL fuer eines, das fehlt oder nicht zu laden war.
///
typedef struct
{
    RAD_IsoTileSpriteInfo_t info;
    SDL_Texture *image;
    SDL_Texture *decor[RAD_ISO_TILE_DECOR_MAX];
} RAD_IsoTileSlot_t;

static RAD_IsoTileSlot_t rad_iso_tile_slots[RAD_ISO_TILE_TYPES];

///
/// Der Lesepuffer fuer eine tile.json, eins groesser als erlaubt: so faellt eine
/// zu grosse Datei auf, ohne dass sie ganz gelesen werden muss.
///
static char rad_iso_tile_json[RAD_ISO_TILE_SPRITE_JSON_MAX + 1];

static void RAD_IsoTileReport(const char *key, const char *reason, const char *detail)
{
    printf("Tile \"%s\": %s%s%s\n", key, reason, (detail != NULL) ? " " : "", (detail != NULL) ? detail : "");
}

///
/// Liest die tile.json von "key" nach "info". Fehlt sie, ist das kein Fehler:
/// "info" bleibt leer, das Feld wird als Wuerfel gezeichnet. false mit einer
/// Meldung, wenn sie zu gross oder ungueltig ist.
///
static bool RAD_IsoTileReadInfo(const char *key, RAD_IsoTileSpriteInfo_t *info)
{
    char path[RAD_ISO_TILE_PATH_MAX];
    if(!RAD_IsoTileSpritePath(key, "tile.json", path, sizeof(path)))
    {
        RAD_IsoTileReport(key, "ist kein gueltiger Ordner", NULL);
        return false;
    }

    SDL_RWops *file = SDL_RWFromFile(path, "rb");
    if(file == NULL)
    {
        return true;
    }
    const size_t length = SDL_RWread(file, rad_iso_tile_json, 1, sizeof(rad_iso_tile_json));
    SDL_RWclose(file);

    const RAD_IsoTileSpriteResult_t result = RAD_IsoTileSpriteParse(rad_iso_tile_json, length, info);
    if(result != RAD_ISO_TILE_SPRITE_OK)
    {
        RAD_IsoTileReport(key, "tile.json ist", RAD_IsoTileSpriteResultText(result));
        return false;
    }
    return true;
}

///
/// Laedt die Datei "file" des Typs "key"; NULL fuer keine (leerer Name) und, mit
/// Meldung, fuer eine, die nicht zu laden ist.
///
static SDL_Texture* RAD_IsoTileLoadTexture(SDL_Renderer *renderer, const char *key, const char *file, bool *complete)
{
    if(file[0] == '\0')
    {
        return NULL;
    }

    char path[RAD_ISO_TILE_PATH_MAX];
    SDL_Texture *texture = NULL;
    if(RAD_IsoTileSpritePath(key, file, path, sizeof(path)))
    {
        texture = IMG_LoadTexture(renderer, path);
    }
    if(texture == NULL)
    {
        RAD_IsoTileReport(key, IMG_GetError(), file);
        *complete = false;
    }
    return texture;
}

static bool RAD_IsoTileLoadSlot(SDL_Renderer *renderer, const char *key, RAD_IsoTileSlot_t *slot)
{
    if(!RAD_IsoTileReadInfo(key, &slot->info))
    {
        memset(&slot->info, 0, sizeof(slot->info));
        return false;
    }

    bool complete = true;
    slot->image = RAD_IsoTileLoadTexture(renderer, key, slot->info.image, &complete);
    for(int32_t i = 0; i < slot->info.number_of_decor; ++i)
    {
        slot->decor[i] = RAD_IsoTileLoadTexture(renderer, key, slot->info.decor[i].image, &complete);
    }
    return complete;
}

bool RAD_IsoTileImagesLoad(SDL_Renderer *renderer)
{
    RAD_IsoTileImagesFree();

    bool complete = true;
    for(int32_t type = 0; type < RAD_ISO_TILE_TYPES; ++type)
    {
        if(RAD_ISO_TILE_KEYS[type] != NULL)
        {
            complete = RAD_IsoTileLoadSlot(renderer, RAD_ISO_TILE_KEYS[type], &rad_iso_tile_slots[type]) && complete;
        }
    }
    return complete;
}

static void RAD_IsoTileDestroy(SDL_Texture **texture)
{
    if(*texture != NULL)
    {
        SDL_DestroyTexture(*texture);
        *texture = NULL;
    }
}

void RAD_IsoTileImagesFree(void)
{
    for(int32_t type = 0; type < RAD_ISO_TILE_TYPES; ++type)
    {
        RAD_IsoTileSlot_t *slot = &rad_iso_tile_slots[type];
        RAD_IsoTileDestroy(&slot->image);
        for(int32_t i = 0; i < RAD_ISO_TILE_DECOR_MAX; ++i)
        {
            RAD_IsoTileDestroy(&slot->decor[i]);
        }
        memset(&slot->info, 0, sizeof(slot->info));
    }
}

void RAD_IsoTileImagesApply(RAD_ClientTileType_t type, RAD_IsoObject_t *object,
                            RAD_IsoDecor_t decor[RAD_ISO_DECOR_PER_TILE])
{
    if((type < 0) || (type >= RAD_ISO_TILE_TYPES))
    {
        return;
    }

    const RAD_IsoTileSlot_t *slot = &rad_iso_tile_slots[type];
    object->image = slot->image;

    // Eine Dekoration, deren Bild nicht zu laden war, laesst keine Luecke.
    int32_t placed = 0;
    for(int32_t i = 0; i < slot->info.number_of_decor; ++i)
    {
        if(slot->decor[i] == NULL)
        {
            continue;
        }
        decor[placed++] = (RAD_IsoDecor_t){
            .image = slot->decor[i],
            .offset_x = slot->info.decor[i].offset_x,
            .offset_y = slot->info.decor[i].offset_y,
            .in_front = slot->info.decor[i].in_front
        };
    }
}
