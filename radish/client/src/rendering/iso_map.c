#include "radish/game/control/events/event_manager.h"
#include <radish/rendering/iso_map.h>
#include <radish/rendering/iso_definitions.h>
#include <stdlib.h>
#include <stddef.h>

#include <radish/game/model/tile/tile.h>

#define RAD_MAP_INDEX_2D(x, y) ((y) * (RAD_ISO_MAP_SIZE) + (x))

static int RAD_MapDepthComparator(const void *a, const void *b);

static void RAD_IsoMapOnTileAddedCallback(void *user_argument, const RAD_Tile_t* p);
static void RAD_IsoMapOnTileRemovedCallback(void *user_argument, const RAD_Tile_t* p);
static void RAD_IsoMapOnTileStateChangedCallback(void *user_argument,const RAD_Tile_t* p);

static void RAD_IsoMapOnEntitySpawned(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y);
static void RAD_IsoMapOnEntityDestroyed(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y);
static void RAD_IsoMapOnEntityMoved(void *user_argument, const RAD_Entity_t *entity, const RAD_EntityPath_t *path, int32_t result);

static RAD_IsoObject_t* RAD_IsoMapObjectOfEntity(RAD_IsoMap_t *map, RAD_EntityId_t id);
static bool RAD_IsoMapInBounds(int32_t x, int32_t y);

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
    map->data[layer][y][x] = RAD_CreateIsoObject(x, y, layer);
    object = &map->data[layer][y][x];
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
        return &map->data[0][y][x]; 

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

    if((entity == NULL) || !RAD_IsoMapInBounds(x, y))
    {
        return;
    }

    RAD_IsoObject_t* object = &map->data[0][y][x];

    RAD_IsoEntity_t *iso_entity = malloc(sizeof(RAD_IsoEntity_t));
    if(iso_entity == NULL)
    {
        return;
    }

    // Die Id mitnehmen, sonst ist die Figur hier namenlos -- und
    // RAD_IsoMapOnEntityMoved findet sie nicht wieder, weil das Move-Ereignis nur
    // sagt, wohin sie ging, und nicht, woher (siehe dort).
    iso_entity->id = entity->id;

    object->entity = iso_entity;
}

static void RAD_IsoMapOnEntityDestroyed(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y)
{
    printf("IsoMap: entity destroyed\n");
}

///
/// Haengt das Iso-Objekt einer Figur auf ihr neues Feld um.
///
/// **Beide Enden stehen im Pfad**, seit er mit dem Startfeld anfaengt (path.h):
/// steps_to[0] ist das verlassene Feld, steps_to[number_of_steps-1] das erreichte.
/// Damit ist das Umhaengen ein Zugriff und keine Suche -- vorher gab das Ereignis
/// nur das Ziel her, und woher die Figur kam, musste diese Map aus ihrer eigenen
/// Buchfuehrung holen.
///
/// **Der gelaufene Weg und nicht der gemeinte.** "path" ist, was tatsaechlich
/// zustande kam, nicht was im Kommando stand: war ein Feld besetzt, hoert er dort
/// auf. Gezeichnet wird also, wie weit die Figur kam.
///
/// Gesucht wird trotzdem, wenn steps_to[0] nichts hergibt -- ein Ereignis, dessen
/// Startfeld leer ist, heisst nicht, dass diese Map die Figur nicht hat. Der Weg
/// ueber die Id (rendering/entity.h) bleibt deshalb als Rueckfall stehen.
///
/// "result" wird nicht ausgewertet, und das ist eine Feststellung ueber heute und
/// keine Absicht: das Spiel setzt es auf 0, ob der Weg ganz gelaufen wurde oder
/// unterwegs endete, und nur bei einer unbekannten Figur auf -1. Als "hat es
/// geklappt" ist es damit nicht zu gebrauchen. Was zaehlt, steht ohnehin im Pfad --
/// weniger als zwei Felder heisst, nichts hat sich bewegt.
///
static void RAD_IsoMapOnEntityMoved(void *user_argument, const RAD_Entity_t *entity, const RAD_EntityPath_t *path, int32_t result)
{
    (void)result;

    RAD_IsoMap_t *map = (RAD_IsoMap_t*)user_argument;

    // Beides kann ausbleiben: bei einer Figur, die es nicht gibt, veroeffentlicht
    // das Spiel mit entity == NULL und einem leeren Pfad (game.c). Ohne diese
    // Pruefung waere das hier ein Absturz und kein verworfenes Ereignis.
    //
    // Unter zwei Feldern ist kein Weg beschrieben (path.h) -- dann hat sich nichts
    // bewegt, und es ist nichts umzuhaengen.
    if((entity == NULL) || (path == NULL) || (path->number_of_steps < 2))
    {
        return;
    }

    const RAD_EntityPosition_t from = path->steps_to[0];
    const RAD_EntityPosition_t to = path->steps_to[path->number_of_steps - 1];

    if(!RAD_IsoMapInBounds(to.x, to.y))
    {
        // Die Welt und dieses Raster sind zwei Dinge: wie gross die Welt ist, liegt
        // privat im Spielmodul, RAD_ISO_MAP_SIZE steht hier. Solange beide acht
        // sind, kann das nicht eintreten -- verlassen wird sich darauf nicht.
        printf("IsoMap: Ziel (%d,%d) liegt ausserhalb des Rasters\n", (int)to.x, (int)to.y);
        return;
    }

    RAD_IsoObject_t *source = NULL;
    if(RAD_IsoMapInBounds(from.x, from.y) && (map->data[0][from.y][from.x].entity != NULL))
    {
        source = &map->data[0][from.y][from.x];
    }
    else
    {
        // Das Startfeld gibt nichts her. Kein Fehler: diese Map kann die Figur
        // woanders haben, etwa weil sie ein frueheres Ereignis verpasst hat.
        source = RAD_IsoMapObjectOfEntity(map, entity->id);
    }

    if(source == NULL)
    {
        printf("IsoMap: Figur %d nicht im Raster\n", (int)entity->id);
        return;
    }

    RAD_IsoObject_t *destination = &map->data[0][to.y][to.x];
    if(destination == source)
    {
        return;
    }

    destination->entity = source->entity;
    source->entity = NULL;
}

///
/// Sucht das Feld, auf dem diese Map die Figur gerade zeichnet, oder NULL.
///
/// Eine Schleife ueber das Raster, weil es keine Zuordnung Figur -> Feld gibt: das
/// Raster ist sie. Auf 8x8 sind das vierundsechzig Vergleiche je Bewegung, und eine
/// Bewegung entsteht durch einen Klick -- billiger als eine zweite Buchfuehrung, die
/// mit der ersten auseinanderlaufen kann.
///
/// Nur Ebene 0: dort setzt RAD_IsoMapOnEntitySpawned die Figuren ab.
///
static RAD_IsoObject_t* RAD_IsoMapObjectOfEntity(RAD_IsoMap_t *map, RAD_EntityId_t id)
{
    for(int32_t y = 0; y < RAD_ISO_MAP_SIZE; ++y)
    {
        for(int32_t x = 0; x < RAD_ISO_MAP_SIZE; ++x)
        {
            RAD_IsoObject_t *object = &map->data[0][y][x];
            if((object->entity != NULL) && (object->entity->id == id))
            {
                return object;
            }
        }
    }

    return NULL;
}

static bool RAD_IsoMapInBounds(int32_t x, int32_t y)
{
    return (x >= 0) && (x < RAD_ISO_MAP_SIZE) && (y >= 0) && (y < RAD_ISO_MAP_SIZE);
}

