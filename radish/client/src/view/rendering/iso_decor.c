#include <radish/view/rendering/iso_decor.h>
#include <stddef.h>


void RAD_IsoDecorPlace(RAD_IsoDecor_t *decor, const RAD_IsoObject_t *tile)
{
    if(decor->image == NULL)
    {
        return;
    }

    decor->tile = tile;
    decor->screen_x = tile->screen_x + decor->offset_x;
    decor->screen_y = tile->screen_y + decor->offset_y;

    // Die Tiefe ihres Feldes, nicht die ihres Rahmens (view/rendering/iso_decor.h).
    decor->depth = (float)tile->screen_y;
}

void RAD_RenderIsoDecor(SDL_Renderer *renderer, const RAD_IsoDecor_t *decor, const RAD_Camera_t *camera)
{
    if((decor->image == NULL) || (decor->tile == NULL))
    {
        return;
    }

    RAD_RenderIsoImage(renderer, decor->image, decor->tile->focus, decor->tile->victory, decor->tile->light,
                       decor->screen_x - camera->x, decor->screen_y - camera->y);
}
