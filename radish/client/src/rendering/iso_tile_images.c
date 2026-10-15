#include <SDL2/SDL_image.h>
#include <stdio.h>
#include <radish/rendering/iso_tile_images.h>

///
/// Wo die Bilder im eingebetteten Dateisystem liegen, je RAD_IsoTileImage_t.
///
static const char *RAD_ISO_TILE_IMAGE_PATHS[RAD_ISO_TILE_IMAGES] = {
    [RAD_ISO_TILE_IMAGE_GRASS] = "/assets/tiles/grass_tile.png"
};

static SDL_Texture *rad_iso_tile_images[RAD_ISO_TILE_IMAGES] = { NULL };

bool RAD_IsoTileImagesLoad(SDL_Renderer *renderer)
{
    bool complete = true;

    for(int32_t i = 0; i < RAD_ISO_TILE_IMAGES; ++i)
    {
        if(rad_iso_tile_images[i] != NULL)
        {
            continue;
        }

        rad_iso_tile_images[i] = IMG_LoadTexture(renderer, RAD_ISO_TILE_IMAGE_PATHS[i]);
        if(rad_iso_tile_images[i] == NULL)
        {
            printf("IMG_LoadTexture %s: %s\n", RAD_ISO_TILE_IMAGE_PATHS[i], IMG_GetError());
            complete = false;
        }
    }

    return complete;
}

void RAD_IsoTileImagesFree(void)
{
    for(int32_t i = 0; i < RAD_ISO_TILE_IMAGES; ++i)
    {
        if(rad_iso_tile_images[i] != NULL)
        {
            SDL_DestroyTexture(rad_iso_tile_images[i]);
            rad_iso_tile_images[i] = NULL;
        }
    }
}

SDL_Texture* RAD_IsoTileImage(RAD_IsoTileImage_t image)
{
    if(image < 0 || image >= RAD_ISO_TILE_IMAGES)
    {
        return NULL;
    }
    return rad_iso_tile_images[image];
}
