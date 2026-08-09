#ifndef __RAD_RENDER_OBJECT_H__
#define __RAD_RENDER_OBJECT_H__


#include "SDL2/SDL_events.h"
#include <SDL2/SDL.h>
#include <stdint.h>
#include <stdbool.h>


struct RAD_RenderObject;
typedef struct RAD_RenderObject* RAD_RenderObject_t;


typedef void (*RAD_RenderCallback_t)(SDL_Renderer *renderer);


typedef struct
{

} RAD_RenderObjectMouseEvent_t;

RAD_RenderObject_t RAD_CreateRenderObject(void);
void RAD_DestroyRenderObject(RAD_RenderObject_t *object);

void RAD_RenderObjectSetPosition(RAD_RenderObject_t object, int32_t x, int32_t y);
void RAD_RenderObjectSetDimensions(RAD_RenderObject_t object, int32_t width, int32_t height);
void RAD_RenderObjectSetBackgroundColor(RAD_RenderObject_t object, SDL_Color color);

void RAD_RenderObjectRender(SDL_Renderer * renderer, RAD_RenderObject_t object);

void RAD_RenderObjectAddRenderObject(RAD_RenderObject_t object, RAD_RenderObject_t new_object);

RAD_RenderObject_t RAD_RenderObjectGetObjectAt(RAD_RenderObject_t object, int32_t x, int32_t y);
bool RAD_RenderObjectContains(const RAD_RenderObject_t object, int32_t x, int32_t y);

#endif

