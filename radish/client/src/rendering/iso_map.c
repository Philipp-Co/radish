#include "radish/game/events/event_manager.h"
#include <radish/rendering/iso_map.h>
#include <radish/rendering/iso_definitions.h>
#include <stdlib.h>
#include <stddef.h>

#include <radish/game/tile.h>

#define RAD_MAP_INDEX_2D(x, y) ((y) * (RAD_ISO_MAP_SIZE) + (x))

static int RAD_MapDepthComparator(const void *a, const void *b);

static void RAD_IsoMapOnTileAddedCallback(void *user_argument, const RAD_Tile_t* p);
static void RAD_IsoMapOnTileRemovedCallback(void *user_argument, const RAD_Tile_t* p);
static void RAD_IsoMapOnTileStateChangedCallback(void *user_argument,const RAD_Tile_t* p);

static void RAD_IsoMapOnEntitySpawned(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y);
static void RAD_IsoMapOnEntityDestroyed(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y);
static void RAD_IsoMapOnEntityMoved(void *user_argument, const RAD_Entity_t *entity, int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y);

RAD_IsoMap_t* RAD_CreateIsoMap(RAD_EventManager_t *manager)
{
    RAD_IsoMap_t *map = malloc(sizeof(RAD_IsoMap_t));
    map->event_manager = manager;
    RAD_EventManagerSubscribeToTileEvents(manager, (RAD_EventsTileChangedCallback_t){
        .user_argument=map,
        .added=RAD_IsoMapOnTileAddedCallback,
        .removed=RAD_IsoMapOnTileRemovedCallback,
        .changed=RAD_IsoMapOnTileStateChangedCallback
    });
    RAD_EventManagerSubscribeToEntityEvents(
        manager,
        (RAD_EventsEntityChangedCallback_t){
            .user_argument=map,
            .destroyed=RAD_IsoMapOnEntityDestroyed,
            .spawned=RAD_IsoMapOnEntitySpawned,
            .moved=RAD_IsoMapOnEntityMoved
        }
    );

    map->number_of_iso_objects = 0;
    
    /*
    for(int32_t y=0;y < 2; ++y)
    {
        for(int32_t x=0;x < 2; ++x)
        {
            RAD_MapAddIsoObject(map, x, y, 0);
        }
    }
    for(int32_t y=2;y < RAD_ISO_MAP_SIZE; ++y)
    {
        for(int32_t x=2;x < RAD_ISO_MAP_SIZE; ++x)
        {
            RAD_MapAddIsoObject(map, x, y, 1);
            //map->lo[y][x]   = RAD_CreateIsoObject(x, y);
            
            //map->mid[y][x]  = RAD_CreateIsoObject(x, y);
            //map->mid[y][x].background_color.a = 0;
            //map->mid[y][x].front_color.a = 0;

            //map->hi[y][x]   = RAD_CreateIsoObject(x, y);
            //map->hi[y][x].background_color.a = 0;
            //map->hi[y][x].front_color.a = 0;
            
            //map->render_order_lo[RAD_MAP_INDEX_2D(x, y)]    = &map->lo[y][x];
            //map->render_order_mid[RAD_MAP_INDEX_2D(x, y)]   = &map->mid[y][x];
            //map->render_order_hi[RAD_MAP_INDEX_2D(x, y)]    = &map->hi[y][x];
        }
    } 
    
    RAD_MapAddIsoObject(map, 4, 5, 2);
    RAD_MapAddIsoObject(map, 4, 2, 2);
    RAD_MapAddIsoObject(map, 4, 3, 2);
    
    RAD_MapAddIsoObject(map, 4, RAD_ISO_MAP_SIZE-1, 2);
    
    qsort(
        map->iso_objects, map->number_of_iso_objects, sizeof(RAD_IsoObject_t*), RAD_MapDepthComparator 
    );
    */
    map->camera.x = 0;
    map->camera.y = 0;
    return map;
}

RAD_IsoObject_t* RAD_MapAddIsoObject(RAD_IsoMap_t *map, int32_t x, int32_t y, int32_t layer)
{
    RAD_IsoObject_t *object;
    switch(layer)
    {
        case 0:
            map->lo[y][x] = RAD_CreateIsoObject(x, y, layer);
            object = &map->lo[y][x];
            break;
        case 1:
            map->mid[y][x] = RAD_CreateIsoObject(x, y, layer);
            object = &map->mid[y][x];
            break;
        case 2:
            map->hi[y][x] = RAD_CreateIsoObject(x, y, layer);
            object = &map->hi[y][x];
            break;
        default:
            object = NULL;
            break;
    }
    object->entity = NULL;
    map->iso_objects[map->number_of_iso_objects++] = object; 
    return object;
}

void RAD_ToFlatCoordinates(RAD_IsoMap_t *map, const int32_t screen_x, const int32_t screen_y, int32_t *x, int32_t *y)
{
    double sx, sy;

    sx = screen_x - (RAD_ISO_TILE_WIDTH / 2) + map->camera.x;
    sy = screen_y - (RAD_ISO_TILE_HEIGHT / 2) + map->camera.y;

    *x = round(((sx / RAD_ISO_TILE_WIDTH) - (sy / RAD_ISO_TILE_HEIGHT)));
    *y = round(((sx / RAD_ISO_TILE_WIDTH) + (sy / RAD_ISO_TILE_HEIGHT)));
}

RAD_IsoObject_t* RAD_IsoObjectAtScreenCoordinates(RAD_IsoMap_t *map, int32_t screen_x, int32_t screen_y)
{
    double sx, sy;

    sx = screen_x - (RAD_ISO_TILE_WIDTH / 2) + map->camera.x;
    sy = screen_y - (RAD_ISO_TILE_HEIGHT / 2) + map->camera.y;

    int32_t x = round(((sx / RAD_ISO_TILE_WIDTH) - (sy / RAD_ISO_TILE_HEIGHT)));
    int32_t y = round(((sx / RAD_ISO_TILE_WIDTH) + (sy / RAD_ISO_TILE_HEIGHT)));

    if(x >= 0 && x < RAD_ISO_MAP_SIZE && y >= 0 && y < RAD_ISO_MAP_SIZE)
    {
        return &map->lo[y][x]; 

    }
    return NULL;
}

void RAD_RenderIsoMap(SDL_Renderer *renderer, RAD_IsoMap_t *map)
{
    for(int32_t i=0;i<map->number_of_iso_objects;++i)
    {
        RAD_RenderIsoObject(renderer, map->iso_objects[i], 0, 0, &map->camera);
    }
    /*
    for(int32_t i=0;i < (MAP_RENDER_SIZE * MAP_RENDER_SIZE); ++i)
    {
        RAD_RenderIsoObject(renderer, map->lo[i], 0, 0, &map->camera);
    }   
    for(int32_t i=0;i < (MAP_RENDER_SIZE * MAP_RENDER_SIZE); ++i)
    {
        RAD_RenderIsoObject(renderer, map->mid[i], 0, -RAD_ISO_TILE_HEIGHT, &map->camera);
    }
    for(int32_t i=0;i < (MAP_RENDER_SIZE * MAP_RENDER_SIZE); ++i)
    {
        //RAD_RenderIsoObject(renderer, map->hi[i], 0, -RAD_ISO_TILE_HEIGHT, &map->camera);
    }
    */

    //for(int32_t i=0;i < (MAP_RENDER_SIZE * MAP_RENDER_SIZE); ++i)
    //{
    //    RAD_RenderIsoObject(renderer, map->render_order_mid[i], 0, -RAD_ISO_TILE_HEIGHT, &map->camera);
    //}   
}

static int RAD_MapDepthComparator(const void *a, const void *b)
{
    const RAD_IsoObject_t **object_a = (const RAD_IsoObject_t**)a;
    const RAD_IsoObject_t **object_b = (const RAD_IsoObject_t**)b;
    if((*object_a)->layer == (*object_b)->layer)
    {
        return (*object_a)->screen_y - (*object_b)->screen_y;
    }
    else if((*object_a)->layer < (*object_b)->layer)
    {
        return -1; 
    }
    else
    {
        return 1;
    }
}

static void RAD_IsoMapOnTileAddedCallback(void *user_argument, const RAD_Tile_t* p)
{
    RAD_IsoMap_t *map = (RAD_IsoMap_t*)user_argument;
    printf("Tile added: %i, %i\n", p->x, p->y);
    RAD_MapAddIsoObject(map, p->x, p->y, p->z);
    qsort(
        map->iso_objects, map->number_of_iso_objects, sizeof(RAD_IsoObject_t*), RAD_MapDepthComparator 
    );
}

static void RAD_IsoMapOnTileRemovedCallback(void *user_argument,  const RAD_Tile_t* p)
{
    RAD_IsoMap_t *map = (RAD_IsoMap_t*)user_argument;
    printf("Tile removed: %i, %i\n", p->x, p->y);
}

static void RAD_IsoMapOnTileStateChangedCallback(void *user_argument, const RAD_Tile_t* p)
{
    RAD_IsoMap_t *map = (RAD_IsoMap_t*)user_argument;
    printf("Tile changed: %i, %i\n", p->x, p->y);
}

static void RAD_IsoMapOnEntitySpawned(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y)
{
    printf("IsoMap: entity spawned\n");
    RAD_IsoMap_t *map = (RAD_IsoMap_t*)user_argument;
    RAD_IsoObject_t* object = &map->lo[y][x];
    
    RAD_IsoEntity_t *iso_entity = malloc(sizeof(RAD_IsoEntity_t));
    object->entity = iso_entity;
}

static void RAD_IsoMapOnEntityDestroyed(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y)
{
    printf("IsoMap: entity destroyed\n");
}

static void RAD_IsoMapOnEntityMoved(void *user_argument, const RAD_Entity_t *entity, int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y)
{
    printf("IsoMap: entity moved\n");
}

