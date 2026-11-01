#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdio.h>
#include <strings.h>
#include <radish/rendering/iso_unit_images.h>

///
/// Ein Atlas, wie er in seiner Datei aufgeteilt ist.
///
typedef struct
{
    ///
    /// Wo er im eingebetteten Dateisystem liegt; NULL: diesen Atlas hat die
    /// Einheit nicht.
    ///
    const char *path;

    ///
    /// Groesse einer Zelle, in Pixeln des Atlas, und wie viele Zellen belegt
    /// sind -- zeilenweise von links oben, eine volle Drehung.
    ///
    int32_t frame_width;
    int32_t frame_height;
    int32_t frame_count;

    ///
    /// Welche Zelle die Drehung bei 0, 90, 180 und 270 Grad zeigt
    /// (rendering/iso_facing.h), aufsteigend durch den Atlas; nach der letzten
    /// Zelle kommt wieder die erste, "frame_count" waere 360 Grad.
    ///
    int32_t key_frames[RAD_ISO_FACINGS];
} RAD_IsoUnitAtlasInfo_t;

///
/// Was zu einem Bild gehoert: seine Atlanten, wie gross und wo die Figur
/// gezeichnet wird, wie schnell sie zieht, sich dreht und zielt.
///
typedef struct
{
    RAD_IsoUnitAtlasInfo_t atlases[RAD_ISO_UNIT_ATLASES];

    ///
    /// Wo die Figur in ihrer Zelle auf dem Boden steht (RAD_IsoUnitFrameTarget),
    /// als Anteil an Breite und Hoehe der Zelle -- so bleibt er bei einem Atlas
    /// in anderer Aufloesung derselbe.
    ///
    float anchor_x;
    float anchor_y;

    ///
    /// Wie breit eine Zelle auf dem Bildschirm gezeichnet wird, in Pixeln; die
    /// Hoehe folgt im Seitenverhaeltnis der Zellen der Drehung.
    ///
    int32_t draw_width;

    ///
    /// Ob der Schatten schon in den Bildern steckt (RAD_IsoUnitHasOwnShadow).
    ///
    bool own_shadow;

    ///
    /// Wie lange ein Schritt von Feld zu Feld, eine Vierteldrehung der Figur und
    /// eine des Turms dauern, und wie lange der Turm nach einem Angriff auf dem
    /// Ziel bleibt (RAD_IsoUnitStepMs, RAD_IsoUnitTurnMs, RAD_IsoUnitTurretMs,
    /// RAD_IsoUnitFireHoldMs), in Millisekunden. 0 heisst: kein eigener Wert.
    ///
    float step_ms;
    float turn_ms;
    float turret_ms;
    float fire_hold_ms;
} RAD_IsoUnitImageInfo_t;

///
/// Der schwere Panzer: die volle Drehung in 396 Zellen zu 128 x 128, der Turm
/// je Blickrichtung in 196, gezeichnet in Breite einer Raute
/// (RAD_ISO_TILE_WIDTH). Die Zellen bei 0/90/180/270 Grad sind fuer die
/// Drehung die, die den frueheren Einzelbildern ht_0/90/180/270 am naechsten
/// kommen, fuer den Turm die Schluesselbilder des Renderings; die Abstaende
/// sind nicht ganz gleich. Der Anker ist die Mitte der frueheren Anker je
/// Richtung.
///
static const RAD_IsoUnitImageInfo_t RAD_ISO_UNIT_IMAGE_INFOS[RAD_ISO_UNIT_SPRITES] = {
    [RAD_ISO_UNIT_SPRITE_HEAVY_TANK] = {
        .atlases = {
            [RAD_ISO_UNIT_ATLAS_TURN] = {
                .path = "/assets/units/heavy_tank/turn.png",
                .frame_width = 128, .frame_height = 128, .frame_count = 396,
                .key_frames = { 0, 100, 198, 298 }
            },
            [RAD_ISO_UNIT_ATLAS_TURRET_X_MINUS] = {
                .path = "/assets/units/heavy_tank/turret_rotation_0.png",
                .frame_width = 128, .frame_height = 128, .frame_count = 196,
                .key_frames = { 0, 49, 96, 147 }
            },
            [RAD_ISO_UNIT_ATLAS_TURRET_Y_PLUS] = {
                .path = "/assets/units/heavy_tank/turret_rotation_90.png",
                .frame_width = 128, .frame_height = 128, .frame_count = 196,
                .key_frames = { 0, 49, 96, 147 }
            },
            [RAD_ISO_UNIT_ATLAS_TURRET_X_PLUS] = {
                .path = "/assets/units/heavy_tank/turret_rotation_180.png",
                .frame_width = 128, .frame_height = 128, .frame_count = 196,
                .key_frames = { 0, 49, 96, 147 }
            },
            [RAD_ISO_UNIT_ATLAS_TURRET_Y_MINUS] = {
                .path = "/assets/units/heavy_tank/turret_rotation_270.png",
                .frame_width = 128, .frame_height = 128, .frame_count = 196,
                .key_frames = { 0, 49, 96, 147 }
            }
        },
        .anchor_x = 0.5f,
        .anchor_y = 0.523f,
        .draw_width = 256,
        .own_shadow = true,
        .step_ms = 750.0f,
        .turn_ms = 1000.0f,
        .turret_ms = 500.0f,
        .fire_hold_ms = 500.0f
    }
};

///
/// Welche Typnamen welches Bild bekommen. Die Typen legt der Katalog an
/// (backend, UnitType.name), ein eigenes Feld dafuer gibt es dort noch nicht.
///
static const struct
{
    const char *type_name;
    RAD_IsoUnitSprite_t sprite;
} RAD_ISO_UNIT_SPRITE_NAMES[] = {
    { "Panzer",          RAD_ISO_UNIT_SPRITE_HEAVY_TANK },
    { "Kampfpanzer",     RAD_ISO_UNIT_SPRITE_HEAVY_TANK },
    { "Schwerer Panzer", RAD_ISO_UNIT_SPRITE_HEAVY_TANK },
    { "Heavy Tank",      RAD_ISO_UNIT_SPRITE_HEAVY_TANK }
};

///
/// Die geladenen Atlanten und wie viele Zellen in einer Zeile liegen -- das
/// sagt erst die Breite der Datei.
///
static SDL_Texture *rad_iso_unit_atlases[RAD_ISO_UNIT_SPRITES][RAD_ISO_UNIT_ATLASES] = { { NULL } };
static int32_t rad_iso_unit_atlas_columns[RAD_ISO_UNIT_SPRITES][RAD_ISO_UNIT_ATLASES] = { { 0 } };

static bool RAD_IsoUnitSpriteValid(RAD_IsoUnitSprite_t sprite)
{
    return (sprite >= 0) && (sprite < RAD_ISO_UNIT_SPRITES);
}

static bool RAD_IsoUnitAtlasValid(RAD_IsoUnitAtlas_t atlas)
{
    return (atlas >= 0) && (atlas < RAD_ISO_UNIT_ATLASES);
}

RAD_IsoUnitSprite_t RAD_IsoUnitSpriteOf(const char *type_name)
{
    if(type_name == NULL)
    {
        return RAD_ISO_UNIT_SPRITE_NONE;
    }

    for(size_t i = 0; i < sizeof(RAD_ISO_UNIT_SPRITE_NAMES) / sizeof(RAD_ISO_UNIT_SPRITE_NAMES[0]); ++i)
    {
        if(strcasecmp(type_name, RAD_ISO_UNIT_SPRITE_NAMES[i].type_name) == 0)
        {
            return RAD_ISO_UNIT_SPRITE_NAMES[i].sprite;
        }
    }
    return RAD_ISO_UNIT_SPRITE_NONE;
}

///
/// "value" aus der Tabelle, oder "fallback" fuer ein Bild ohne eigenen Wert (0).
///
static float RAD_IsoUnitDuration(float value, float fallback)
{
    return (value > 0.0f) ? value : fallback;
}

float RAD_IsoUnitStepMs(RAD_IsoUnitSprite_t sprite)
{
    return RAD_IsoUnitSpriteValid(sprite)
           ? RAD_IsoUnitDuration(RAD_ISO_UNIT_IMAGE_INFOS[sprite].step_ms, RAD_ISO_UNIT_STEP_MS_DEFAULT)
           : RAD_ISO_UNIT_STEP_MS_DEFAULT;
}

float RAD_IsoUnitTurnMs(RAD_IsoUnitSprite_t sprite)
{
    return RAD_IsoUnitSpriteValid(sprite)
           ? RAD_IsoUnitDuration(RAD_ISO_UNIT_IMAGE_INFOS[sprite].turn_ms, RAD_ISO_UNIT_TURN_MS_DEFAULT)
           : RAD_ISO_UNIT_TURN_MS_DEFAULT;
}

float RAD_IsoUnitTurretMs(RAD_IsoUnitSprite_t sprite)
{
    return RAD_IsoUnitSpriteValid(sprite)
           ? RAD_IsoUnitDuration(RAD_ISO_UNIT_IMAGE_INFOS[sprite].turret_ms, RAD_ISO_UNIT_TURRET_MS_DEFAULT)
           : RAD_ISO_UNIT_TURRET_MS_DEFAULT;
}

float RAD_IsoUnitFireHoldMs(RAD_IsoUnitSprite_t sprite)
{
    return RAD_IsoUnitSpriteValid(sprite)
           ? RAD_IsoUnitDuration(RAD_ISO_UNIT_IMAGE_INFOS[sprite].fire_hold_ms, RAD_ISO_UNIT_FIRE_HOLD_MS_DEFAULT)
           : RAD_ISO_UNIT_FIRE_HOLD_MS_DEFAULT;
}

RAD_IsoUnitAtlas_t RAD_IsoUnitTurretAtlas(RAD_IsoFacing_t facing)
{
    if(facing < 0 || facing >= RAD_ISO_FACINGS)
    {
        return RAD_ISO_UNIT_ATLASES;
    }
    return (RAD_IsoUnitAtlas_t)(RAD_ISO_UNIT_ATLAS_TURRET_X_MINUS + facing);
}

///
/// Wie viele Zellen in einer Zeile des Atlas "texture" fuer "info" liegen; 0,
/// wenn der Atlas nicht zu den Angaben passt -- zu klein fuer alle Zellen,
/// oder eine der vier festen Zellen liegt nicht in ihm oder nicht aufsteigend.
///
static int32_t RAD_IsoUnitAtlasColumns(const RAD_IsoUnitAtlasInfo_t *info, SDL_Texture *texture)
{
    int32_t w = 0, h = 0;
    SDL_QueryTexture(texture, NULL, NULL, &w, &h);
    if(info->frame_width <= 0 || info->frame_height <= 0 || info->frame_count <= 0)
    {
        return 0;
    }

    const int32_t columns = w / info->frame_width;
    const int32_t rows = h / info->frame_height;
    if(columns * rows < info->frame_count)
    {
        printf("Atlas %s: %d x %d Zellen, %d gebraucht\n", info->path, (int)columns, (int)rows, (int)info->frame_count);
        return 0;
    }

    for(int32_t i = 0; i < RAD_ISO_FACINGS; ++i)
    {
        const int32_t previous = (i > 0) ? info->key_frames[i - 1] : -1;
        if(info->key_frames[i] <= previous || info->key_frames[i] >= info->frame_count)
        {
            printf("Atlas %s: %d Grad auf Zelle %d\n", info->path, (int)(90 * i), (int)info->key_frames[i]);
            return 0;
        }
    }
    return columns;
}

bool RAD_IsoUnitImagesLoad(SDL_Renderer *renderer)
{
    bool complete = true;

    for(int32_t sprite = 0; sprite < RAD_ISO_UNIT_SPRITES; ++sprite)
    {
        for(int32_t atlas = 0; atlas < RAD_ISO_UNIT_ATLASES; ++atlas)
        {
            const RAD_IsoUnitAtlasInfo_t *info = &RAD_ISO_UNIT_IMAGE_INFOS[sprite].atlases[atlas];
            if(info->path == NULL || rad_iso_unit_atlases[sprite][atlas] != NULL)
            {
                continue;
            }

            SDL_Texture *texture = IMG_LoadTexture(renderer, info->path);
            if(texture == NULL)
            {
                printf("IMG_LoadTexture %s: %s\n", info->path, IMG_GetError());
                complete = false;
                continue;
            }

            const int32_t columns = RAD_IsoUnitAtlasColumns(info, texture);
            if(columns == 0)
            {
                SDL_DestroyTexture(texture);
                complete = false;
                continue;
            }

            // Die Zellen werden vergroessert oder verkleinert gezeichnet; ohne das
            // nimmt SDL das naechste Pixel, und die Figur wird kantig.
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
            rad_iso_unit_atlases[sprite][atlas] = texture;
            rad_iso_unit_atlas_columns[sprite][atlas] = columns;
        }
    }

    return complete;
}

void RAD_IsoUnitImagesFree(void)
{
    for(int32_t sprite = 0; sprite < RAD_ISO_UNIT_SPRITES; ++sprite)
    {
        for(int32_t atlas = 0; atlas < RAD_ISO_UNIT_ATLASES; ++atlas)
        {
            if(rad_iso_unit_atlases[sprite][atlas] != NULL)
            {
                SDL_DestroyTexture(rad_iso_unit_atlases[sprite][atlas]);
                rad_iso_unit_atlases[sprite][atlas] = NULL;
                rad_iso_unit_atlas_columns[sprite][atlas] = 0;
            }
        }
    }
}

SDL_Texture* RAD_IsoUnitAtlasTexture(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAtlas_t atlas)
{
    if(!RAD_IsoUnitSpriteValid(sprite) || !RAD_IsoUnitAtlasValid(atlas))
    {
        return NULL;
    }
    return rad_iso_unit_atlases[sprite][atlas];
}

bool RAD_IsoUnitHasTurret(RAD_IsoUnitSprite_t sprite)
{
    for(int32_t facing = 0; facing < RAD_ISO_FACINGS; ++facing)
    {
        if(RAD_IsoUnitAtlasTexture(sprite, RAD_IsoUnitTurretAtlas((RAD_IsoFacing_t)facing)) == NULL)
        {
            return false;
        }
    }
    return true;
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
    if(!RAD_IsoUnitSpriteValid(sprite) || facing < 0 || facing >= RAD_ISO_FACINGS)
    {
        return -1;
    }
    return RAD_ISO_UNIT_IMAGE_INFOS[sprite].atlases[RAD_ISO_UNIT_ATLAS_TURN].key_frames[facing];
}

int32_t RAD_IsoUnitTurnFrame(RAD_IsoUnitSprite_t sprite, RAD_IsoFacing_t from, RAD_IsoFacing_t to, float progress)
{
    if(RAD_IsoUnitFacingFrame(sprite, from) < 0 || RAD_IsoUnitFacingFrame(sprite, to) < 0)
    {
        return -1;
    }

    const float clamped = fminf(fmaxf(progress, 0.0f), 1.0f);
    const int32_t turns = RAD_IsoFacingQuarterTurns(from, to);
    const float angle = RAD_IsoFacingAngle(from) + clamped * 90.0f * (float)turns;
    return RAD_IsoUnitAtlasFrameAt(&RAD_ISO_UNIT_IMAGE_INFOS[sprite].atlases[RAD_ISO_UNIT_ATLAS_TURN], angle);
}

int32_t RAD_IsoUnitTurretFrame(RAD_IsoUnitSprite_t sprite, RAD_IsoFacing_t facing, float angle)
{
    const RAD_IsoUnitAtlas_t atlas = RAD_IsoUnitTurretAtlas(facing);
    if(!RAD_IsoUnitSpriteValid(sprite) || !RAD_IsoUnitAtlasValid(atlas)
       || RAD_ISO_UNIT_IMAGE_INFOS[sprite].atlases[atlas].frame_count <= 0)
    {
        return -1;
    }
    return RAD_IsoUnitAtlasFrameAt(&RAD_ISO_UNIT_IMAGE_INFOS[sprite].atlases[atlas], angle);
}

bool RAD_IsoUnitFrameSource(RAD_IsoUnitSprite_t sprite, RAD_IsoUnitAtlas_t atlas, int32_t frame, SDL_Rect *source)
{
    if(RAD_IsoUnitAtlasTexture(sprite, atlas) == NULL)
    {
        return false;
    }

    const RAD_IsoUnitAtlasInfo_t *info = &RAD_ISO_UNIT_IMAGE_INFOS[sprite].atlases[atlas];
    if(frame < 0 || frame >= info->frame_count)
    {
        return false;
    }

    const int32_t columns = rad_iso_unit_atlas_columns[sprite][atlas];
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
    if(!RAD_IsoUnitSpriteValid(sprite))
    {
        return false;
    }

    const RAD_IsoUnitImageInfo_t *info = &RAD_ISO_UNIT_IMAGE_INFOS[sprite];
    const RAD_IsoUnitAtlasInfo_t *turn = &info->atlases[RAD_ISO_UNIT_ATLAS_TURN];
    const int32_t w = info->draw_width;
    const int32_t h = (turn->frame_width > 0)
                      ? (int32_t)lroundf((float)w * (float)turn->frame_height / (float)turn->frame_width)
                      : 0;
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
    return RAD_IsoUnitSpriteValid(sprite) && RAD_ISO_UNIT_IMAGE_INFOS[sprite].own_shadow;
}
