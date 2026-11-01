#include <SDL2/SDL_image.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <radish/rendering/iso_effect_images.h>

///
/// Die Folge einer Richtung: in welchem Atlas sie liegt, wie er aufgeteilt
/// ist, und wo darin der Anker liegt.
///
typedef struct
{
    ///
    /// Wo der Atlas im eingebetteten Dateisystem liegt; NULL: diese Richtung
    /// hat der Effekt nicht. Richtungen mit demselben Pfad teilen sich den
    /// geladenen Atlas.
    ///
    const char *path;

    ///
    /// Groesse einer Zelle, in Pixeln des Atlas, mit welcher Zelle die Folge
    /// beginnt und wie viele Zellen sie belegt -- zeilenweise von links oben.
    ///
    int32_t frame_width;
    int32_t frame_height;
    int32_t first_frame;
    int32_t frame_count;

    ///
    /// Wo der Anker in der Zelle liegt (RAD_IsoEffectFrameTarget), als Anteil an
    /// Breite und Hoehe der Zelle.
    ///
    float anchor_x;
    float anchor_y;
} RAD_IsoEffectAtlasInfo_t;

///
/// Was zu einem Effekt gehoert: seine Atlanten je Richtung, wie gross er
/// gezeichnet wird und wie lange er laeuft.
///
typedef struct
{
    RAD_IsoEffectAtlasInfo_t atlases[RAD_ISO_FACINGS];

    ///
    /// Wie breit eine Zelle auf dem Bildschirm gezeichnet wird, in Pixeln; die
    /// Hoehe folgt im Seitenverhaeltnis der Zelle.
    ///
    int32_t draw_width;

    float duration_ms;
} RAD_IsoEffectImageInfo_t;

///
/// Die Explosionen, alle in Zellen zu 128 x 128, im selben Massstab gerendert
/// wie der schwere Panzer und darum wie er in doppelter Breite gezeichnet. Der
/// Einschlag liegt nicht in der Mitte der Zelle und je Richtung woanders; der
/// Anker ist die Mitte des Feuerballs in seinem ersten Bild. Alle laufen
/// gleich lang, der Turm bleibt so lange auf dem Ziel (view/iso_map_view.c).
///
///   - explosion_0: 146 Zellen, in alle Richtungen gleich.
///   - explosion_1: je Richtung eine Datei mit 150 Zellen.
///   - explosion_2: die vier Richtungen nacheinander in einer Datei, je 146
///     Zellen, in der Reihenfolge 0, 90, 180, 270 Grad.
///
/// **Die Zahl einer Richtung ist die Gegenrichtung des Schusses**: in "0"
/// fliegt der Feuerball nach X_PLUS (180 Grad), in "90" nach Y_MINUS, in "180"
/// nach X_MINUS, in "270" nach Y_PLUS. Er soll vom Schuetzen weg fliegen, also
/// in Schussrichtung -- darum zeigt ein Schuss nach X_MINUS das Bild "180" und
/// so fort, um 180 Grad versetzt.
///
#define RAD_ISO_EXPLOSION_0(facing) \
    [facing] = { \
        .path = "/assets/effects/explosion_0.png", \
        .frame_width = 128, .frame_height = 128, .first_frame = 0, .frame_count = 146, \
        .anchor_x = 0.586f, .anchor_y = 0.711f \
    }

static const RAD_IsoEffectImageInfo_t RAD_ISO_EFFECT_IMAGE_INFOS[RAD_ISO_EFFECT_SPRITES] = {
    [RAD_ISO_EFFECT_SPRITE_EXPLOSION_0] = {
        .atlases = {
            RAD_ISO_EXPLOSION_0(RAD_ISO_FACING_X_MINUS),
            RAD_ISO_EXPLOSION_0(RAD_ISO_FACING_Y_PLUS),
            RAD_ISO_EXPLOSION_0(RAD_ISO_FACING_X_PLUS),
            RAD_ISO_EXPLOSION_0(RAD_ISO_FACING_Y_MINUS)
        },
        .draw_width = 256,
        .duration_ms = 2500.0f
    },
    [RAD_ISO_EFFECT_SPRITE_EXPLOSION_1] = {
        .atlases = {
            [RAD_ISO_FACING_X_PLUS] = {
                .path = "/assets/effects/explosion_1/0.png",
                .frame_width = 128, .frame_height = 128, .first_frame = 0, .frame_count = 150,
                .anchor_x = 0.496f, .anchor_y = 0.695f
            },
            [RAD_ISO_FACING_Y_MINUS] = {
                .path = "/assets/effects/explosion_1/90.png",
                .frame_width = 128, .frame_height = 128, .first_frame = 0, .frame_count = 150,
                .anchor_x = 0.926f, .anchor_y = 0.680f
            },
            [RAD_ISO_FACING_X_MINUS] = {
                .path = "/assets/effects/explosion_1/180.png",
                .frame_width = 128, .frame_height = 128, .first_frame = 0, .frame_count = 150,
                .anchor_x = 0.914f, .anchor_y = 0.582f
            },
            [RAD_ISO_FACING_Y_PLUS] = {
                .path = "/assets/effects/explosion_1/270.png",
                .frame_width = 128, .frame_height = 128, .first_frame = 0, .frame_count = 150,
                .anchor_x = 0.715f, .anchor_y = 0.633f
            }
        },
        .draw_width = 256,
        .duration_ms = 2500.0f
    },
    [RAD_ISO_EFFECT_SPRITE_EXPLOSION_2] = {
        .atlases = {
            [RAD_ISO_FACING_X_PLUS] = {
                .path = "/assets/effects/explosion_2.png",
                .frame_width = 128, .frame_height = 128, .first_frame = 0, .frame_count = 146,
                .anchor_x = 0.414f, .anchor_y = 0.883f
            },
            [RAD_ISO_FACING_Y_MINUS] = {
                .path = "/assets/effects/explosion_2.png",
                .frame_width = 128, .frame_height = 128, .first_frame = 146, .frame_count = 146,
                .anchor_x = 0.863f, .anchor_y = 0.844f
            },
            [RAD_ISO_FACING_X_MINUS] = {
                .path = "/assets/effects/explosion_2.png",
                .frame_width = 128, .frame_height = 128, .first_frame = 292, .frame_count = 146,
                .anchor_x = 0.902f, .anchor_y = 0.559f
            },
            [RAD_ISO_FACING_Y_PLUS] = {
                .path = "/assets/effects/explosion_2.png",
                .frame_width = 128, .frame_height = 128, .first_frame = 438, .frame_count = 146,
                .anchor_x = 0.492f, .anchor_y = 0.750f
            }
        },
        .draw_width = 256,
        .duration_ms = 2500.0f
    }
};

#undef RAD_ISO_EXPLOSION_0

///
/// Aus diesen waehlt RAD_IsoEffectRandomExplosion.
///
static const RAD_IsoEffectSprite_t RAD_ISO_EFFECT_EXPLOSIONS[] = {
    RAD_ISO_EFFECT_SPRITE_EXPLOSION_0,
    RAD_ISO_EFFECT_SPRITE_EXPLOSION_1,
    RAD_ISO_EFFECT_SPRITE_EXPLOSION_2
};

///
/// Die geladenen Atlanten je Effekt und Richtung und wie viele Zellen in einer
/// Zeile liegen -- das sagt erst die Breite der Datei. Richtungen mit demselben
/// Pfad zeigen auf dieselbe Textur.
///
static SDL_Texture *rad_iso_effect_atlases[RAD_ISO_EFFECT_SPRITES][RAD_ISO_FACINGS] = { { NULL } };
static int32_t rad_iso_effect_atlas_columns[RAD_ISO_EFFECT_SPRITES][RAD_ISO_FACINGS] = { { 0 } };

static bool RAD_IsoEffectValid(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing)
{
    return (sprite >= 0) && (sprite < RAD_ISO_EFFECT_SPRITES) && (facing >= 0) && (facing < RAD_ISO_FACINGS);
}

RAD_IsoEffectSprite_t RAD_IsoEffectRandomExplosion(void)
{
    const size_t count = sizeof(RAD_ISO_EFFECT_EXPLOSIONS) / sizeof(RAD_ISO_EFFECT_EXPLOSIONS[0]);
    return RAD_ISO_EFFECT_EXPLOSIONS[(size_t)rand() % count];
}

///
/// Wie viele Zellen in einer Zeile des Atlas "texture" fuer "info" liegen; 0,
/// wenn der Atlas zu klein fuer die Folge ist.
///
static int32_t RAD_IsoEffectAtlasColumns(const RAD_IsoEffectAtlasInfo_t *info, SDL_Texture *texture)
{
    int32_t w = 0, h = 0;
    SDL_QueryTexture(texture, NULL, NULL, &w, &h);
    if(info->frame_width <= 0 || info->frame_height <= 0 || info->frame_count <= 0 || info->first_frame < 0)
    {
        return 0;
    }

    const int32_t columns = w / info->frame_width;
    const int32_t rows = h / info->frame_height;
    if(columns * rows < info->first_frame + info->frame_count)
    {
        printf("Atlas %s: %d x %d Zellen, %d gebraucht\n", info->path, (int)columns, (int)rows,
               (int)(info->first_frame + info->frame_count));
        return 0;
    }
    return columns;
}

///
/// Der schon geladene Atlas mit dem Pfad "path", oder NULL.
///
static SDL_Texture* RAD_IsoEffectLoadedAtlas(const char *path)
{
    for(int32_t sprite = 0; sprite < RAD_ISO_EFFECT_SPRITES; ++sprite)
    {
        for(int32_t facing = 0; facing < RAD_ISO_FACINGS; ++facing)
        {
            const char *other = RAD_ISO_EFFECT_IMAGE_INFOS[sprite].atlases[facing].path;
            if((rad_iso_effect_atlases[sprite][facing] != NULL) && (other != NULL) && (strcmp(other, path) == 0))
            {
                return rad_iso_effect_atlases[sprite][facing];
            }
        }
    }
    return NULL;
}

bool RAD_IsoEffectImagesLoad(SDL_Renderer *renderer)
{
    bool complete = true;

    for(int32_t sprite = 0; sprite < RAD_ISO_EFFECT_SPRITES; ++sprite)
    {
        for(int32_t facing = 0; facing < RAD_ISO_FACINGS; ++facing)
        {
            const RAD_IsoEffectAtlasInfo_t *info = &RAD_ISO_EFFECT_IMAGE_INFOS[sprite].atlases[facing];
            if(info->path == NULL || rad_iso_effect_atlases[sprite][facing] != NULL)
            {
                continue;
            }

            SDL_Texture *texture = RAD_IsoEffectLoadedAtlas(info->path);
            const bool shared = (texture != NULL);
            if(!shared)
            {
                texture = IMG_LoadTexture(renderer, info->path);
            }
            if(texture == NULL)
            {
                printf("IMG_LoadTexture %s: %s\n", info->path, IMG_GetError());
                complete = false;
                continue;
            }

            const int32_t columns = RAD_IsoEffectAtlasColumns(info, texture);
            if(columns == 0)
            {
                if(!shared)
                {
                    SDL_DestroyTexture(texture);
                }
                complete = false;
                continue;
            }

            // Wie bei den Einheiten: vergroessert gezeichnet, ohne das kantig.
            SDL_SetTextureScaleMode(texture, SDL_ScaleModeLinear);
            rad_iso_effect_atlases[sprite][facing] = texture;
            rad_iso_effect_atlas_columns[sprite][facing] = columns;
        }
    }

    return complete;
}

void RAD_IsoEffectImagesFree(void)
{
    for(int32_t sprite = 0; sprite < RAD_ISO_EFFECT_SPRITES; ++sprite)
    {
        for(int32_t facing = 0; facing < RAD_ISO_FACINGS; ++facing)
        {
            SDL_Texture *texture = rad_iso_effect_atlases[sprite][facing];
            if(texture == NULL)
            {
                continue;
            }

            // Geteilte Atlanten nur einmal freigeben: alle Plaetze, die auf
            // dieselbe Textur zeigen, werden dabei mit geleert.
            SDL_DestroyTexture(texture);
            for(int32_t other_sprite = 0; other_sprite < RAD_ISO_EFFECT_SPRITES; ++other_sprite)
            {
                for(int32_t other_facing = 0; other_facing < RAD_ISO_FACINGS; ++other_facing)
                {
                    if(rad_iso_effect_atlases[other_sprite][other_facing] == texture)
                    {
                        rad_iso_effect_atlases[other_sprite][other_facing] = NULL;
                        rad_iso_effect_atlas_columns[other_sprite][other_facing] = 0;
                    }
                }
            }
        }
    }
}

float RAD_IsoEffectDurationMs(RAD_IsoEffectSprite_t sprite)
{
    if((sprite < 0) || (sprite >= RAD_ISO_EFFECT_SPRITES))
    {
        return 0.0f;
    }
    return RAD_ISO_EFFECT_IMAGE_INFOS[sprite].duration_ms;
}

SDL_Texture* RAD_IsoEffectAtlasTexture(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing)
{
    if(!RAD_IsoEffectValid(sprite, facing))
    {
        return NULL;
    }
    return rad_iso_effect_atlases[sprite][facing];
}

int32_t RAD_IsoEffectFrameAt(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing, float progress)
{
    if(!RAD_IsoEffectValid(sprite, facing))
    {
        return -1;
    }

    const int32_t count = RAD_ISO_EFFECT_IMAGE_INFOS[sprite].atlases[facing].frame_count;
    if(count <= 0)
    {
        return -1;
    }

    const float clamped = fminf(fmaxf(progress, 0.0f), 1.0f);
    const int32_t frame = (int32_t)(clamped * (float)count);
    return (frame < count) ? frame : (count - 1);
}

bool RAD_IsoEffectFrameSource(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing, int32_t frame, SDL_Rect *source)
{
    if(RAD_IsoEffectAtlasTexture(sprite, facing) == NULL)
    {
        return false;
    }

    const RAD_IsoEffectAtlasInfo_t *info = &RAD_ISO_EFFECT_IMAGE_INFOS[sprite].atlases[facing];
    if(frame < 0 || frame >= info->frame_count)
    {
        return false;
    }

    const int32_t columns = rad_iso_effect_atlas_columns[sprite][facing];
    const int32_t cell = info->first_frame + frame;
    *source = (SDL_Rect){
        .x = (cell % columns) * info->frame_width,
        .y = (cell / columns) * info->frame_height,
        .w = info->frame_width,
        .h = info->frame_height
    };
    return true;
}

bool RAD_IsoEffectFrameTarget(RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing, int32_t x, int32_t y,
                              SDL_Rect *target)
{
    if(!RAD_IsoEffectValid(sprite, facing))
    {
        return false;
    }

    const RAD_IsoEffectImageInfo_t *info = &RAD_ISO_EFFECT_IMAGE_INFOS[sprite];
    const RAD_IsoEffectAtlasInfo_t *atlas = &info->atlases[facing];
    const int32_t w = info->draw_width;
    const int32_t h = (atlas->frame_width > 0)
                      ? (int32_t)lroundf((float)w * (float)atlas->frame_height / (float)atlas->frame_width)
                      : 0;
    *target = (SDL_Rect){
        .x = x - (int32_t)lroundf(atlas->anchor_x * (float)w),
        .y = y - (int32_t)lroundf(atlas->anchor_y * (float)h),
        .w = w,
        .h = h
    };
    return true;
}
