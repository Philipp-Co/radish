#ifndef __RAD_MAP_H__
#define __RAD_MAP_H__

#include <SDL2/SDL_render.h>
#include <radish/rendering/iso_object.h>
#include <radish/rendering/iso_definitions.h>
#include <radish/rendering/camera.h>

typedef struct
{
    RAD_IsoObject_t *iso_object;
} RAD_Object_t;

typedef struct
{
    RAD_IsoObject_t hi    [RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];    
    RAD_IsoObject_t mid   [RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];    
    RAD_IsoObject_t lo    [RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];    
   
    int32_t number_of_iso_objects; 
    RAD_IsoObject_t *iso_objects[RAD_ISO_MAP_SIZE * RAD_ISO_MAP_SIZE * 3];

    RAD_Camera_t camera;
} RAD_IsoMap_t;


RAD_IsoMap_t* RAD_CreateIsoMap();
RAD_IsoObject_t* RAD_IsoObjectAtScreenCoordinates(RAD_IsoMap_t *map, int32_t screen_x, int32_t srceen_y);
void RAD_RenderIsoMap(SDL_Renderer *renderer, RAD_IsoMap_t *map);
RAD_IsoObject_t* RAD_MapAddIsoObject(RAD_IsoMap_t *map, int32_t x, int32_t y, int32_t layer);


#endif
