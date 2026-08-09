#include "SDL2/SDL_error.h"
#include "SDL2/SDL_render.h"
#include <radish/rendering/render_object.h>
#include <radish/math.h>

#include <stdlib.h>
#include <stdio.h>
#include <math.h>


struct RAD_RenderObjectListItem;

struct RAD_RenderObjectListItem
{
    RAD_RenderObject_t object;
    struct RAD_RenderObjectListItem *next;
};

typedef struct
{
    struct RAD_RenderObjectListItem *root;
} RAD_RenderObjectList_t;

struct RAD_RenderObject
{
    SDL_Color background_color;
    struct RAD_RenderObject *parent;
    int32_t x;
    int32_t y;
    int32_t width;
    int32_t height;
    RAD_RenderObjectList_t children;
};


RAD_RenderObject_t RAD_CreateRenderObject(void)
{
    RAD_RenderObject_t object = malloc(sizeof(struct RAD_RenderObject));
    object->x = 0;
    object->y = 0;
    object->width = 0;
    object->height = 0;
    object->parent = NULL;
    RAD_RenderObjectList_t children = {
        .root = NULL
    };
    object->background_color.a = 255;
    object->background_color.a = 255;
    object->background_color.a = 255;
    object->background_color.a = 0;
    object->children = children;
    return object;
}

void RAD_DestroyRenderObject(RAD_RenderObject_t *object)
{
    if(NULL == object)
    {
        return;
    }
    struct RAD_RenderObjectListItem *item = (*object)->children.root;
    while(NULL != item)
    {
        RAD_DestroyRenderObject(&item->object);
        struct RAD_RenderObjectListItem *old_item = item;
        item = item->next; 
        free(old_item);
    }
    free(*object);
    *object = NULL;
}

void RAD_RenderObjectSetBackgroundColor(RAD_RenderObject_t object, SDL_Color color)
{
    object->background_color = color;
}

void RAD_RenderObjectSetPosition(RAD_RenderObject_t object, int32_t x, int32_t y)
{
    object->x = x;
    object->y = y;
}

void RAD_RenderObjectSetDimensions(RAD_RenderObject_t object, int32_t width, int32_t height)
{
    object->width = width;
    object->height = height;
}

// Clippt rekursiv auf den ueberschneidenden Bereich aller Vorfahren, statt Breite/Height
// der Kinder rechnerisch zu verzerren -- SDL_RenderSetClipRect verwirft Pixel ausserhalb
// automatisch, ohne dass die gespeicherten Objekt-Bounds veraendert werden muessen.
static void RAD_RenderObjectRenderClipped(SDL_Renderer *renderer, RAD_RenderObject_t object, const SDL_Rect *inherited_clip)
{
    SDL_RenderSetClipRect(renderer, inherited_clip);

    SDL_Rect rect = {
        .x=object->x,
        .y=object->y,
        .w=object->width,
        .h=object->height
    };

    SDL_SetRenderDrawColor(
        renderer, object->background_color.r, object->background_color.g, object->background_color.b, object->background_color.a
    );
    if(0 != SDL_RenderFillRect(renderer, &rect))
    {
        printf("Error during rendering of Object: %s\n", SDL_GetError());
    }

    SDL_Rect children_clip = rect;
    if(NULL != inherited_clip)
    {
        SDL_IntersectRect(inherited_clip, &rect, &children_clip);
    }

    struct RAD_RenderObjectListItem *item = object->children.root;
    while(NULL != item)
    {
        RAD_RenderObjectRenderClipped(renderer, item->object, &children_clip);
        item = item->next;
    }
}

void RAD_RenderObjectRender(SDL_Renderer * renderer, RAD_RenderObject_t object)
{
    RAD_RenderObjectRenderClipped(renderer, object, NULL);
    // Clip-Rect zuruecksetzen, damit nachfolgendes Zeichnen (z.B. Status-Anzeige,
    // Log-Text) ausserhalb dieses Objektbaums nicht versehentlich mitgeclippt wird.
    SDL_RenderSetClipRect(renderer, NULL);
}

void RAD_RenderObjectAddRenderObject(RAD_RenderObject_t object, RAD_RenderObject_t new_object)
{
    struct RAD_RenderObjectListItem *new_item = malloc(sizeof(struct RAD_RenderObjectListItem));
    new_item->object = new_object;
    new_item->next = NULL;

    // x/y wurden vor dem Hinzufuegen relativ zum (kuenftigen) Elternobjekt gesetzt --
    // beim Einhaengen einmalig in absolute Koordinaten umrechnen.
    new_object->parent = object;
    new_object->x = object->x + new_object->x;
    new_object->y = object->y + new_object->y;

    if(NULL == object->children.root)
    {
        object->children.root = new_item;
    }
    else
    {
        struct RAD_RenderObjectListItem *item = object->children.root;
        while(NULL != item->next)
        {
            item = item->next;
        }
        item->next = new_item;
    }
}

RAD_RenderObject_t RAD_RenderObjectGetObjectAt(RAD_RenderObject_t object, int32_t x, int32_t y)
{
    struct RAD_RenderObjectListItem *item = object->children.root;
    while(NULL != item)
    {
        if(RAD_RenderObjectContains(item->object, x, y))
        {
            return RAD_RenderObjectGetObjectAt(item->object, x, y);
        }
        item = item->next;
    }
    return object;
}

bool RAD_RenderObjectContains(const RAD_RenderObject_t object, int32_t x, int32_t y)
{
    return object->x < x && x < (object->x + object->width) && object->y < y && y < (object->y + object->height); 
}
