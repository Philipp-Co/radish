#include <radish/rendering/iso_map.h>
#include <radish/rendering/iso_definitions.h>
#include <radish/rendering/iso_tile.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#define RAD_MAP_INDEX_2D(x, y) ((y) * (RAD_ISO_MAP_SIZE) + (x))

static int RAD_MapDepthComparator(const void *a, const void *b);

#define RAD_ISO_MAP_LAYERS 3

static void RAD_IsoMapRemoveObject(RAD_IsoMap_t *map, RAD_IsoObject_t *object);
static void RAD_IsoMapSetEntity(RAD_IsoMap_t *map, int32_t x, int32_t y, RAD_NetEntityId_t id);

static void RAD_IsoMapFreeEntities(RAD_IsoObject_t *object);
static bool RAD_IsoMapInBounds(int32_t x, int32_t y);

RAD_IsoMap_t* RAD_CreateIsoMap(void)
{
    // calloc: jedes Objekt beginnt als nicht eingetragen (present == false) und
    // ohne Figuren (entities[] == NULL) -- RAD_IsoMapApplyTile verlaesst sich darauf.
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

void RAD_DestroyIsoMap(RAD_IsoMap_t **map)
{
    RAD_IsoMap_t *m = *map;

    // Figuren in der Zeichenliste gehoeren dem, der sie eingetragen hat
    // (RAD_IsoMapAddEntity) -- nur die auf den Feldern gehoeren der Map.
    for(int32_t layer = 0; layer < RAD_ISO_MAP_LAYERS; ++layer)
    {
        for(int32_t y = 0; y < RAD_ISO_MAP_SIZE; ++y)
        {
            for(int32_t x = 0; x < RAD_ISO_MAP_SIZE; ++x)
            {
                RAD_IsoMapFreeEntities(&m->data[layer][y][x]);
            }
        }
    }

    free(m);
    *map = NULL;
}

RAD_IsoObject_t* RAD_MapAddIsoObject(RAD_IsoMap_t *map, int32_t x, int32_t y, int32_t layer)
{
    RAD_IsoObject_t *object;
    // Die Figuren bleiben stehen, in jeder Tiefe: sie folgen dem Feld und nicht
    // seinem Iso-Objekt (RAD_IsoMapSetEntity).
    RAD_IsoEntity_t *entities[RAD_ISO_DEPTHS];
    memcpy(entities, map->data[layer][y][x].entities, sizeof(entities));
    map->data[layer][y][x] = RAD_CreateIsoObject(x, y, layer);
    object = &map->data[layer][y][x];
    memcpy(object->entities, entities, sizeof(entities));
    object->present = true;
    map->iso_objects[map->number_of_iso_objects++] = (RAD_IsoRenderable_t){
        .type = RAD_ISO_RENDERABLE_OBJECT,
        .object = object
    };
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

///
/// Wo eine Figur ausserhalb eines Feldes auf dem Bildschirm steht, ohne Kamera:
/// dieselbe Rechnung wie RAD_CreateIsoObject fuer ein Feld, nur mit
/// Nachkommastellen.
///
static float RAD_IsoMapEntityScreenX(const RAD_IsoEntity_t *entity)
{
    return (entity->x + entity->y) * (float)(RAD_ISO_TILE_WIDTH / 2);
}

static float RAD_IsoMapEntityScreenY(const RAD_IsoEntity_t *entity)
{
    return (entity->y - entity->x) * (float)(RAD_ISO_TILE_HEIGHT / 2);
}

void RAD_RenderIsoMap(SDL_Renderer *renderer, RAD_IsoMap_t *map)
{
    // Wer eine Figur eingetragen hat, darf sie jederzeit versetzen
    // (RAD_IsoMapAddEntity) -- ihre Tiefe stimmt also nur, wenn hier neu sortiert
    // wird. Ohne Figuren aendert sich die Reihenfolge nur beim Eintragen.
    if(map->number_of_entities > 0)
    {
        qsort(
            map->iso_objects, map->number_of_iso_objects, sizeof(RAD_IsoRenderable_t), RAD_MapDepthComparator
        );
    }

    for(int32_t i=0;i<map->number_of_iso_objects;++i)
    {
        const RAD_IsoRenderable_t *renderable = &map->iso_objects[i];
        if(renderable->type == RAD_ISO_RENDERABLE_OBJECT)
        {
            RAD_RenderIsoObject(renderer, renderable->object, 0, 0, &map->camera);
        }
        else
        {
            // Wie RAD_RenderIsoObject: Kamera abziehen, jede Ebene eine Kachel hoeher.
            const RAD_IsoEntity_t *entity = renderable->entity;
            const int32_t screen_x = (int32_t)lroundf(RAD_IsoMapEntityScreenX(entity)) - map->camera.x;
            const int32_t screen_y = (int32_t)lroundf(RAD_IsoMapEntityScreenY(entity)) - map->camera.y
                                     + entity->layer * -RAD_ISO_TILE_HEIGHT;
            RAD_RenderIsoEntity(renderer, entity, screen_x, screen_y);
        }
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

///
/// Ebene und Tiefe eines Eintrags: fuer ein Feld wie angelegt, fuer eine Figur
/// aus ihrer Position (RAD_IsoMapEntityScreenY).
///
static void RAD_MapDepthOf(const RAD_IsoRenderable_t *renderable, int32_t *layer, float *screen_y)
{
    if(renderable->type == RAD_ISO_RENDERABLE_OBJECT)
    {
        *layer = renderable->object->layer;
        *screen_y = (float)renderable->object->screen_y;
    }
    else
    {
        *layer = renderable->entity->layer;
        *screen_y = RAD_IsoMapEntityScreenY(renderable->entity);
    }
}

///
/// Hinten zuerst: nach Ebene, dann nach Tiefe. Steht eine Figur so tief wie ein
/// Feld, kommt das Feld zuerst -- sie liegt darauf. qsort ist nicht stabil;
/// ohne diese Regel koennte sie von Frame zu Frame mal unter, mal ueber ihm
/// landen. Zwei Figuren gleicher Tiefe ordnet ihre Id.
///
static int RAD_MapDepthComparator(const void *a, const void *b)
{
    const RAD_IsoRenderable_t *renderable_a = (const RAD_IsoRenderable_t*)a;
    const RAD_IsoRenderable_t *renderable_b = (const RAD_IsoRenderable_t*)b;

    int32_t layer_a = 0, layer_b = 0;
    float depth_a = 0.0f, depth_b = 0.0f;
    RAD_MapDepthOf(renderable_a, &layer_a, &depth_a);
    RAD_MapDepthOf(renderable_b, &layer_b, &depth_b);

    if(layer_a != layer_b)
    {
        return (layer_a < layer_b) ? -1 : 1;
    }
    if(depth_a != depth_b)
    {
        return (depth_a < depth_b) ? -1 : 1;
    }
    if(renderable_a->type != renderable_b->type)
    {
        return (renderable_a->type == RAD_ISO_RENDERABLE_OBJECT) ? -1 : 1;
    }
    if(renderable_a->type == RAD_ISO_RENDERABLE_ENTITY)
    {
        const RAD_NetEntityId_t id_a = renderable_a->entity->id;
        const RAD_NetEntityId_t id_b = renderable_b->entity->id;
        return (id_a < id_b) ? -1 : (id_a > id_b) ? 1 : 0;
    }
    return 0;
}

bool RAD_IsoMapAddEntity(RAD_IsoMap_t *map, RAD_IsoEntity_t *entity)
{
    if(map->number_of_entities >= RAD_ISO_MAP_ENTITIES_MAX)
    {
        return false;
    }

    for(int32_t i = 0; i < map->number_of_iso_objects; ++i)
    {
        if((map->iso_objects[i].type == RAD_ISO_RENDERABLE_ENTITY) && (map->iso_objects[i].entity == entity))
        {
            return false;
        }
    }

    map->iso_objects[map->number_of_iso_objects++] = (RAD_IsoRenderable_t){
        .type = RAD_ISO_RENDERABLE_ENTITY,
        .entity = entity
    };
    map->number_of_entities++;

    qsort(
        map->iso_objects, map->number_of_iso_objects, sizeof(RAD_IsoRenderable_t), RAD_MapDepthComparator
    );
    return true;
}

void RAD_IsoMapRemoveEntity(RAD_IsoMap_t *map, const RAD_IsoEntity_t *entity)
{
    for(int32_t i = 0; i < map->number_of_iso_objects; ++i)
    {
        if((map->iso_objects[i].type != RAD_ISO_RENDERABLE_ENTITY) || (map->iso_objects[i].entity != entity))
        {
            continue;
        }

        // Verschieben haelt die Reihenfolge der uebrigen, wie bei einem Feld
        // (RAD_IsoMapRemoveObject).
        for(int32_t j = i + 1; j < map->number_of_iso_objects; ++j)
        {
            map->iso_objects[j - 1] = map->iso_objects[j];
        }
        map->number_of_iso_objects--;
        map->number_of_entities--;
        return;
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
            map->iso_objects, map->number_of_iso_objects, sizeof(RAD_IsoRenderable_t), RAD_MapDepthComparator
        );
    }

    // Bei jedem Aufruf und nicht nur beim Anlegen: auch ein Feld, das den Typ
    // wechselt, sieht danach aus wie sein neuer Typ (rendering/iso_tile.h).
    RAD_IsoTileApplyType(&map->data[layer][y][x], tile->type);

    // Ein Siegfeld zeichnet sich selbst anders (RAD_RenderIsoObject).
    map->data[layer][y][x].victory = tile->victory;

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

    // Mit dem Feld gehen alle seine Figuren, nicht nur die Einheit.
    for(int32_t layer = 0; layer < RAD_ISO_MAP_LAYERS; ++layer)
    {
        RAD_IsoMapFreeEntities(&map->data[layer][y][x]);
    }
}

///
/// Nimmt ein Objekt aus der Zeichenreihenfolge. Die Reihenfolge der uebrigen
/// bleibt, sie ist nach Tiefe sortiert.
///
static void RAD_IsoMapRemoveObject(RAD_IsoMap_t *map, RAD_IsoObject_t *object)
{
    for(int32_t i = 0; i < map->number_of_iso_objects; ++i)
    {
        if((map->iso_objects[i].type != RAD_ISO_RENDERABLE_OBJECT) || (map->iso_objects[i].object != object))
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
/// Setzt die Einheit auf Feld (x, y) oder nimmt sie weg (RAD_NET_ENTITY_NONE).
/// Einheiten stehen auf Ebene 0, in RAD_ISO_UNIT_DEPTH; die anderen Tiefen
/// bleiben, wie sie sind.
///
static void RAD_IsoMapSetEntity(RAD_IsoMap_t *map, int32_t x, int32_t y, RAD_NetEntityId_t id)
{
    RAD_IsoEntity_t **unit = &map->data[0][y][x].entities[RAD_ISO_UNIT_DEPTH];

    if(id == RAD_NET_ENTITY_NONE)
    {
        free(*unit);
        *unit = NULL;
        return;
    }

    if(*unit == NULL)
    {
        // calloc: Position und Ebene gelten erst ausserhalb eines Feldes
        // (rendering/entity.h), bis dahin 0.
        *unit = calloc(1, sizeof(RAD_IsoEntity_t));
        if(*unit == NULL)
        {
            return;
        }
    }

    (*unit)->id = id;
}

///
/// Gibt die Figuren eines Feldes in allen Tiefen frei (RAD_IsoObject_t.entities)
/// und laesst die Plaetze leer.
///
static void RAD_IsoMapFreeEntities(RAD_IsoObject_t *object)
{
    for(int32_t depth = 0; depth < RAD_ISO_DEPTHS; ++depth)
    {
        free(object->entities[depth]);
        object->entities[depth] = NULL;
    }
}

static bool RAD_IsoMapInBounds(int32_t x, int32_t y)
{
    return (x >= 0) && (x < RAD_ISO_MAP_SIZE) && (y >= 0) && (y < RAD_ISO_MAP_SIZE);
}

