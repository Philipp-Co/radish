#ifndef __RAD_ISO_OBJECT_H__
#define __RAD_ISO_OBJECT_H__

#include <SDL2/SDL_render.h>
#include <stdint.h>
#include <stdbool.h>
#include <radish/rendering/entity.h>
#include <radish/rendering/iso_definitions.h>
#include <radish/rendering/camera.h>


typedef struct
{
    int32_t x;
    int32_t y;
    int32_t screen_x;
    int32_t screen_y; 

    SDL_Color background_color;
    SDL_Color front_color;

    bool focus;
    int32_t layer;
} RAD_IsoObject_t;


RAD_IsoObject_t RAD_CreateIsoObject(int32_t x, int32_t y, int32_t layer);
void RAD_RenderIsoObject(SDL_Renderer *renderer, RAD_IsoObject_t *object, int32_t offset_x, int32_t offset_y, RAD_Camera_t *camera);

void RAD_ToIsoCoordinates(int32_t x, int32_t y, int32_t *screen_x, int32_t *screen_y);

#endif
