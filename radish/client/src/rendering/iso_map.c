#include <radish/rendering/iso_map.h>
#include <radish/rendering/iso_definitions.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>

#define RAD_MAP_INDEX_2D(x, y) ((y) * (RAD_ISO_MAP_SIZE) + (x))

static int RAD_MapDepthComparator(const void *a, const void *b);

#define RAD_ISO_MAP_LAYERS 3

static void RAD_IsoMapRemoveObject(RAD_IsoMap_t *map, RAD_IsoObject_t *object);
static void RAD_IsoMapSetEntity(RAD_IsoMap_t *map, int32_t x, int32_t y, RAD_NetEntityId_t id);

static RAD_IsoObject_t* RAD_IsoMapObjectOfEntity(RAD_IsoMap_t *map, RAD_NetEntityId_t id);
static bool RAD_IsoMapInBounds(int32_t x, int32_t y);

RAD_IsoMap_t* RAD_CreateIsoMap(void)
{
    // calloc: jedes Objekt beginnt als nicht eingetragen (present == false) und
    // ohne Figur (entity == NULL) -- RAD_IsoMapApplyTile verlaesst sich darauf.
    RAD_IsoMap_t *map = calloc(1, sizeof(RAD_IsoMap_t));
    if(map == NULL)
    {
        return NULL;
    }

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
    // Die Figur bleibt stehen: sie folgt dem Feld und nicht seinem Iso-Objekt
    // (RAD_IsoMapSetEntity).
    RAD_IsoEntity_t *entity = map->data[layer][y][x].entity;
    map->data[layer][y][x] = RAD_CreateIsoObject(x, y, layer);
    object = &map->data[layer][y][x];
    object->entity = entity;
    object->present = true;
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

void RAD_IsoMapVisibleArea(RAD_IsoMap_t *map, int32_t screen_width, int32_t screen_height,
                           int32_t *x, int32_t *y, int32_t *w, int32_t *h)
{
    const int32_t corners[4][2] = {
        { 0,            0             },
        { screen_width, 0             },
        { 0,            screen_height },
        { screen_width, screen_height }
    };

    int32_t min_x = INT32_MAX, min_y = INT32_MAX;
    int32_t max_x = INT32_MIN, max_y = INT32_MIN;
    for(int32_t i = 0; i < 4; ++i)
    {
        int32_t tile_x = 0, tile_y = 0;
        RAD_ToFlatCoordinates(map, corners[i][0], corners[i][1], &tile_x, &tile_y);
        min_x = (tile_x < min_x) ? tile_x : min_x;
        min_y = (tile_y < min_y) ? tile_y : min_y;
        max_x = (tile_x > max_x) ? tile_x : max_x;
        max_y = (tile_y > max_y) ? tile_y : max_y;
    }

    // Auf das Raster begrenzen; max ist dabei das letzte Feld, nicht das erste
    // dahinter.
    min_x = (min_x < 0) ? 0 : min_x;
    min_y = (min_y < 0) ? 0 : min_y;
    max_x = (max_x > RAD_ISO_MAP_SIZE - 1) ? RAD_ISO_MAP_SIZE - 1 : max_x;
    max_y = (max_y > RAD_ISO_MAP_SIZE - 1) ? RAD_ISO_MAP_SIZE - 1 : max_y;

    *x = min_x;
    *y = min_y;
    *w = (max_x >= min_x) ? (max_x - min_x + 1) : 0;
    *h = (max_y >= min_y) ? (max_y - min_y + 1) : 0;
}

void RAD_IsoMapScreenBounds(const RAD_IsoMap_t *map, int32_t *min_x, int32_t *min_y, int32_t *max_x, int32_t *max_y)
{
    (void)map;

    // Die vier Ecken des Rasters, durch dieselbe Rechnung wie beim Anlegen eines
    // Objekts (RAD_CreateIsoObject) -- die Karte ist eine Raute, ihre Extreme
    // liegen an den Ecken.
    const int32_t last = RAD_ISO_MAP_SIZE - 1;
    const int32_t corners[4][2] = { { 0, 0 }, { last, 0 }, { 0, last }, { last, last } };

    int32_t left = INT32_MAX, top = INT32_MAX;
    int32_t right = INT32_MIN, bottom = INT32_MIN;
    for(int32_t i = 0; i < 4; ++i)
    {
        const RAD_IsoObject_t corner = RAD_CreateIsoObject(corners[i][0], corners[i][1], 0);
        left = (corner.screen_x < left) ? corner.screen_x : left;
        top = (corner.screen_y < top) ? corner.screen_y : top;
        right = (corner.screen_x > right) ? corner.screen_x : right;
        bottom = (corner.screen_y > bottom) ? corner.screen_y : bottom;
    }

    // Was ein Objekt ueber seinen Ankerpunkt hinaus zeichnet
    // (RAD_RenderIsoObject): eine Kachel breit, nach unten die Seitenflaechen bis
    // zwei Kachelhoehen, nach oben jede hoehere Ebene eine Kachelhoehe und darauf
    // noch eine halbe fuer die Figur.
    *min_x = left;
    *min_y = top - (RAD_ISO_MAP_LAYERS - 1) * RAD_ISO_TILE_HEIGHT - RAD_ISO_TILE_HEIGHT / 2;
    *max_x = right + RAD_ISO_TILE_WIDTH;
    *max_y = bottom + 2 * RAD_ISO_TILE_HEIGHT;
}

bool RAD_IsoMapTileScreenCenter(const RAD_IsoMap_t *map, int32_t x, int32_t y, int32_t *screen_x, int32_t *screen_y)
{
    if(x < 0 || x >= RAD_ISO_MAP_SIZE || y < 0 || y >= RAD_ISO_MAP_SIZE || !map->data[0][y][x].present)
    {
        return false;
    }

    // Wie RAD_RenderIsoObject: die Raute beginnt bei (screen_x, screen_y) und
    // ist RAD_ISO_TILE_WIDTH x RAD_ISO_TILE_HEIGHT gross.
    const RAD_IsoObject_t *object = &map->data[0][y][x];
    *screen_x = object->screen_x - map->camera.x + RAD_ISO_TILE_WIDTH / 2;
    *screen_y = object->screen_y - map->camera.y + RAD_ISO_TILE_HEIGHT / 2;
    return true;
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

void RAD_IsoMapApplyTile(RAD_IsoMap_t *map, const RAD_ClientTile_t *tile)
{
    const int32_t x = tile->x;
    const int32_t y = tile->y;

    // Das Model kennt keine Hoehe (model/tile.h): alles liegt auf Ebene 0.
    const int32_t layer = 0;

    if(!RAD_IsoMapInBounds(x, y) || (layer >= RAD_ISO_MAP_LAYERS))
    {
        printf("IsoMap: Feld (%d,%d) z=%d liegt ausserhalb des Rasters\n", (int)x, (int)y, (int)layer);
        return;
    }

    // Hat sich die Hoehe geaendert, steht das Feld noch auf einer anderen Ebene.
    for(int32_t other = 0; other < RAD_ISO_MAP_LAYERS; ++other)
    {
        if((other != layer) && map->data[other][y][x].present)
        {
            RAD_IsoMapRemoveObject(map, &map->data[other][y][x]);
        }
    }

    if(!map->data[layer][y][x].present)
    {
        RAD_MapAddIsoObject(map, x, y, layer);
        qsort(
            map->iso_objects, map->number_of_iso_objects, sizeof(RAD_IsoObject_t*), RAD_MapDepthComparator 
        );
    }

    RAD_IsoMapSetEntity(map, x, y, (tile->unit != NULL) ? tile->unit->id : RAD_NET_ENTITY_NONE);
}

void RAD_IsoMapRemoveTile(RAD_IsoMap_t *map, uint32_t x, uint32_t y)
{
    if(!RAD_IsoMapInBounds((int32_t)x, (int32_t)y))
    {
        return;
    }

    for(int32_t layer = 0; layer < RAD_ISO_MAP_LAYERS; ++layer)
    {
        if(map->data[layer][y][x].present)
        {
            RAD_IsoMapRemoveObject(map, &map->data[layer][y][x]);
        }
    }

    RAD_IsoMapSetEntity(map, (int32_t)x, (int32_t)y, RAD_NET_ENTITY_NONE);
}

///
/// Nimmt ein Objekt aus der Zeichenreihenfolge. Die Reihenfolge der uebrigen
/// bleibt, sie ist nach Tiefe sortiert.
///
static void RAD_IsoMapRemoveObject(RAD_IsoMap_t *map, RAD_IsoObject_t *object)
{
    for(int32_t i = 0; i < map->number_of_iso_objects; ++i)
    {
        if(map->iso_objects[i] != object)
        {
            continue;
        }

        for(int32_t j = i + 1; j < map->number_of_iso_objects; ++j)
        {
            map->iso_objects[j - 1] = map->iso_objects[j];
        }
        map->number_of_iso_objects--;
        break;
    }

    object->present = false;
}

///
/// Setzt die Figur auf Feld (x, y) oder nimmt sie weg (RAD_NET_ENTITY_NONE).
/// Figuren stehen auf Ebene 0, wie bisher.
///
static void RAD_IsoMapSetEntity(RAD_IsoMap_t *map, int32_t x, int32_t y, RAD_NetEntityId_t id)
{
    RAD_IsoObject_t *object = &map->data[0][y][x];

    if(id == RAD_NET_ENTITY_NONE)
    {
        free(object->entity);
        object->entity = NULL;
        return;
    }

    if(object->entity == NULL)
    {
        object->entity = malloc(sizeof(RAD_IsoEntity_t));
        if(object->entity == NULL)
        {
            return;
        }
    }

    object->entity->id = id;
}

///
/// Haengt das Iso-Objekt einer Figur auf ihr neues Feld um.
///
/// **Beide Enden stehen im Pfad**, er faengt mit dem Startfeld an
/// (io/net_types.h): steps_to[0] ist das verlassene Feld,
/// steps_to[number_of_steps-1] das erreichte. Damit ist das Umhaengen ein Zugriff
/// und keine Suche.
///
/// Gesucht wird trotzdem, wenn steps_to[0] nichts hergibt -- ein leeres Startfeld
/// heisst nicht, dass diese Map die Figur nicht hat. Der Weg ueber die Id
/// (rendering/entity.h) bleibt deshalb als Rueckfall stehen.
///
void RAD_IsoMapMoveEntity(RAD_IsoMap_t *map, RAD_NetEntityId_t entity, const RAD_NetPath_t *path)
{
    // Unter zwei Feldern ist kein Weg beschrieben (net_types.h) -- dann hat sich
    // nichts bewegt, und es ist nichts umzuhaengen.
    if((entity == RAD_NET_ENTITY_NONE) || (path == NULL) || (path->number_of_steps < 2))
    {
        return;
    }

    const RAD_NetPosition_t from = path->steps_to[0];
    const RAD_NetPosition_t to = path->steps_to[path->number_of_steps - 1];

    if(!RAD_IsoMapInBounds(to.x, to.y))
    {
        // Die Welt und dieses Raster sind zwei Dinge: wie gross die Welt ist,
        // sagt der Server, RAD_ISO_MAP_SIZE steht hier.
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
        source = RAD_IsoMapObjectOfEntity(map, entity);
    }

    if(source == NULL)
    {
        printf("IsoMap: Figur %d nicht im Raster\n", (int)entity);
        return;
    }

    RAD_IsoObject_t *destination = &map->data[0][to.y][to.x];
    if(destination == source)
    {
        return;
    }

    free(destination->entity);
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
/// Nur Ebene 0: dort setzt RAD_IsoMapSetEntity die Figuren ab.
///
static RAD_IsoObject_t* RAD_IsoMapObjectOfEntity(RAD_IsoMap_t *map, RAD_NetEntityId_t id)
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

