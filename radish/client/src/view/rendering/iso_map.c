#include <radish/view/rendering/iso_map.h>
#include <radish/view/rendering/iso_definitions.h>
#include <radish/view/rendering/iso_tile.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

static int RAD_MapDepthComparator(const void *a, const void *b);

static void RAD_IsoMapRemoveObject(RAD_IsoMap_t *map, RAD_IsoObject_t *object);
static void RAD_IsoMapSetEntity(RAD_IsoMap_t *map, int32_t x, int32_t y, const RAD_ClientUnit_t *unit, int32_t player);
static bool RAD_IsoMapAddRenderable(RAD_IsoMap_t *map, RAD_IsoRenderable_t renderable);
static RAD_IsoEntity_t* RAD_IsoMapCreateEntity(RAD_IsoMap_t *map, RAD_NetEntityId_t id, int32_t x, int32_t y);
static bool RAD_IsoMapRemoveRenderable(RAD_IsoMap_t *map, RAD_IsoRenderableType_t type, const void *pointer);
static bool RAD_IsoMapInBounds(int32_t x, int32_t y);
static void RAD_IsoMapApplyDecor(RAD_IsoMap_t *map, int32_t x, int32_t y);
static void RAD_IsoMapRemoveDecor(RAD_IsoMap_t *map, int32_t x, int32_t y);

RAD_IsoMap_t* RAD_CreateIsoMap(void)
{
    // calloc: jedes Objekt beginnt als nicht eingetragen (present == false) und
    // die Map ohne Figuren -- RAD_IsoMapApplyTile verlaesst sich darauf.
    RAD_IsoMap_t *map = calloc(1, sizeof(RAD_IsoMap_t));
    if(map == NULL)
    {
        return NULL;
    }

    map->number_of_iso_objects = 0;
    map->camera.x = 0;
    map->camera.y = 0;
    return map;
}

void RAD_DestroyIsoMap(RAD_IsoMap_t **map)
{
    RAD_IsoMap_t *m = *map;

    // Alle Figuren gehoeren der Map (RAD_IsoMapEntityOf); Effekte nicht.
    for(int32_t i = 0; i < m->number_of_entities; ++i)
    {
        free(m->entities[i]);
    }

    free(m);
    *map = NULL;
}

RAD_IsoObject_t* RAD_MapAddIsoObject(RAD_IsoMap_t *map, int32_t x, int32_t y)
{
    RAD_IsoObject_t *object;
    // Die Figuren bleiben stehen: sie haengen nicht am Iso-Objekt, sondern an
    // der Map (RAD_IsoMapEntityOf).
    map->data[y][x] = RAD_CreateIsoObject(x, y);
    object = &map->data[y][x];
    object->present = true;
    map->iso_objects[map->number_of_iso_objects++] = (RAD_IsoRenderable_t){
        .type = RAD_ISO_RENDERABLE_OBJECT,
        .layer = RAD_ISO_LAYER_TILE,
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
        const RAD_IsoObject_t corner = RAD_CreateIsoObject(corners[i][0], corners[i][1]);
        left = (corner.screen_x < left) ? corner.screen_x : left;
        top = (corner.screen_y < top) ? corner.screen_y : top;
        right = (corner.screen_x > right) ? corner.screen_x : right;
        bottom = (corner.screen_y > bottom) ? corner.screen_y : bottom;
    }

    // Was ueber den Ankerpunkt eines Feldes hinaus gezeichnet wird: eine Kachel
    // breit, nach unten die Seitenflaechen bis zwei Kachelhoehen, nach oben die
    // hoechste Dekoration (RAD_ISO_DECOR_MAX_RISE) und darauf noch eine halbe
    // Kachelhoehe fuer die Figur.
    *min_x = left;
    *min_y = top - RAD_ISO_DECOR_MAX_RISE - RAD_ISO_TILE_HEIGHT / 2;
    *max_x = right + RAD_ISO_TILE_WIDTH;
    *max_y = bottom + 2 * RAD_ISO_TILE_HEIGHT;
}

bool RAD_IsoMapTileScreenCenter(const RAD_IsoMap_t *map, int32_t x, int32_t y, int32_t *screen_x, int32_t *screen_y)
{
    if(x < 0 || x >= RAD_ISO_MAP_SIZE || y < 0 || y >= RAD_ISO_MAP_SIZE || !map->data[y][x].present)
    {
        return false;
    }

    // Wie RAD_RenderIsoObject: die Raute beginnt bei (screen_x, screen_y) und
    // ist RAD_ISO_TILE_WIDTH x RAD_ISO_TILE_HEIGHT gross.
    const RAD_IsoObject_t *object = &map->data[y][x];
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
        return &map->data[y][x]; 

    }
    return NULL;
}

///
/// Wo eine Figur ausserhalb eines Feldes oder ein Effekt bei (x, y) auf dem
/// Bildschirm steht, ohne Kamera: dieselbe Rechnung wie RAD_CreateIsoObject fuer
/// ein Feld, nur mit Nachkommastellen.
///
static float RAD_IsoMapLooseScreenX(float x, float y)
{
    return (x + y) * (float)(RAD_ISO_TILE_WIDTH / 2);
}

static float RAD_IsoMapLooseScreenY(float x, float y)
{
    return (y - x) * (float)(RAD_ISO_TILE_HEIGHT / 2);
}

///
/// Fuehrt eine Figur oder einen Effekt in die Zeichenliste ein; false, wenn
/// er schon darin steht.
///
static bool RAD_IsoMapAddRenderable(RAD_IsoMap_t *map, RAD_IsoRenderable_t renderable)
{
    for(int32_t i = 0; i < map->number_of_iso_objects; ++i)
    {
        if((map->iso_objects[i].type == renderable.type) && (map->iso_objects[i].entity == renderable.entity))
        {
            return false;
        }
    }

    map->iso_objects[map->number_of_iso_objects++] = renderable;
    qsort(
        map->iso_objects, map->number_of_iso_objects, sizeof(RAD_IsoRenderable_t), RAD_MapDepthComparator
    );
    return true;
}

///
/// Nimmt eine Figur oder einen Effekt aus der Zeichenliste; false, wenn er
/// nicht darin stand.
///
static bool RAD_IsoMapRemoveRenderable(RAD_IsoMap_t *map, RAD_IsoRenderableType_t type, const void *pointer)
{
    for(int32_t i = 0; i < map->number_of_iso_objects; ++i)
    {
        if((map->iso_objects[i].type != type) || ((const void*)map->iso_objects[i].entity != pointer))
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
        return true;
    }
    return false;
}

void RAD_RenderIsoMap(SDL_Renderer *renderer, RAD_IsoMap_t *map)
{
    // Jedes Bild neu sortiert: wer eine Figur eingetragen hat, darf sie
    // jederzeit versetzen, ihre Tiefe stimmt also nur,
    // wenn hier sortiert wird.
    qsort(
        map->iso_objects, map->number_of_iso_objects, sizeof(RAD_IsoRenderable_t), RAD_MapDepthComparator
    );

    for(int32_t i=0;i<map->number_of_iso_objects;++i)
    {
        const RAD_IsoRenderable_t *renderable = &map->iso_objects[i];
        if(renderable->type == RAD_ISO_RENDERABLE_OBJECT)
        {
            RAD_RenderIsoObject(renderer, renderable->object, &map->camera);
        }
        else if(renderable->type == RAD_ISO_RENDERABLE_DECOR)
        {
            RAD_RenderIsoDecor(renderer, renderable->decor, &map->camera);
        }
        else if(renderable->type == RAD_ISO_RENDERABLE_ENTITY)
        {
            // Ob auf einem Feld oder nicht: gezeichnet wird bei ihrer Position.
            const RAD_IsoEntity_t *entity = renderable->entity;
            const int32_t screen_x = (int32_t)lroundf(RAD_IsoMapLooseScreenX(entity->x, entity->y)) - map->camera.x;
            const int32_t screen_y = (int32_t)lroundf(RAD_IsoMapLooseScreenY(entity->x, entity->y)) - map->camera.y;
            RAD_RenderIsoEntity(renderer, entity, screen_x, screen_y);
        }
        else
        {
            const RAD_IsoEffect_t *effect = renderable->effect;
            const int32_t screen_x = (int32_t)lroundf(RAD_IsoMapLooseScreenX(effect->x, effect->y)) - map->camera.x;
            const int32_t screen_y = (int32_t)lroundf(RAD_IsoMapLooseScreenY(effect->x, effect->y)) - map->camera.y;
            RAD_RenderIsoEffect(renderer, effect, screen_x, screen_y);
        }
    }
}

///
/// Wo ein Eintrag im Raster steht: ein Feld und seine Dekoration auf dem Feld,
/// eine Figur oder ein Effekt bei ihrer Position, auch zwischen zwei Feldern.
///
static void RAD_MapPositionOf(const RAD_IsoRenderable_t *renderable, float *x, float *y)
{
    switch(renderable->type)
    {
        case RAD_ISO_RENDERABLE_OBJECT:
            *x = (float)renderable->object->x;
            *y = (float)renderable->object->y;
            break;
        case RAD_ISO_RENDERABLE_DECOR:
            // In der Zeichenliste steht sie nur mit Bild, also gesetzt
            // (RAD_IsoDecorPlace).
            *x = (float)renderable->decor->tile->x;
            *y = (float)renderable->decor->tile->y;
            break;
        case RAD_ISO_RENDERABLE_ENTITY:
            *x = renderable->entity->x;
            *y = renderable->entity->y;
            break;
        case RAD_ISO_RENDERABLE_EFFECT:
        default:
            *x = renderable->effect->x;
            *y = renderable->effect->y;
            break;
    }
}

///
/// Der Wert, nach dem sortiert wird: die Reihe (y - x, vorn ist groesser),
/// je Ebene um RAD_ISO_LAYER_STEP nach vorn gerueckt (iso_map.h).
///
static float RAD_MapSortValue(const RAD_IsoRenderable_t *renderable)
{
    float x = 0.0f, y = 0.0f;
    RAD_MapPositionOf(renderable, &x, &y);
    return (y - x) + (float)renderable->layer * RAD_ISO_LAYER_STEP;
}

///
/// Hinten zuerst, allein nach RAD_MapSortValue. Gleich sind nur Eintraege
/// derselben Ebene in derselben Reihe; die ueberlappen sich nicht, ihre
/// Reihenfolge ist egal.
///
static int RAD_MapDepthComparator(const void *a, const void *b)
{
    const float value_a = RAD_MapSortValue((const RAD_IsoRenderable_t*)a);
    const float value_b = RAD_MapSortValue((const RAD_IsoRenderable_t*)b);
    return (value_a > value_b) - (value_a < value_b);
}

RAD_IsoEntity_t* RAD_IsoMapEntityOf(RAD_IsoMap_t *map, RAD_NetEntityId_t id)
{
    for(int32_t i = 0; i < map->number_of_entities; ++i)
    {
        if(map->entities[i]->id == id)
        {
            return map->entities[i];
        }
    }
    return NULL;
}

RAD_IsoEntity_t* RAD_IsoMapEntityAt(RAD_IsoMap_t *map, int32_t x, int32_t y)
{
    for(int32_t i = 0; i < map->number_of_entities; ++i)
    {
        RAD_IsoEntity_t *entity = map->entities[i];

        // Eine Figur in Bewegung steht zwischen den Feldern; nur eine, die
        // genau auf (x, y) steht, steht dort auch.
        if((entity->x == (float)x) && (entity->y == (float)y))
        {
            return entity;
        }
    }
    return NULL;
}

int32_t RAD_IsoMapEntityCount(const RAD_IsoMap_t *map)
{
    return map->number_of_entities;
}

RAD_IsoEntity_t* RAD_IsoMapEntityAtIndex(RAD_IsoMap_t *map, int32_t index)
{
    return ((index >= 0) && (index < map->number_of_entities)) ? map->entities[index] : NULL;
}

void RAD_IsoMapRemoveEntity(RAD_IsoMap_t *map, RAD_NetEntityId_t id)
{
    for(int32_t i = 0; i < map->number_of_entities; ++i)
    {
        RAD_IsoEntity_t *entity = map->entities[i];
        if(entity->id != id)
        {
            continue;
        }

        RAD_IsoMapRemoveRenderable(map, RAD_ISO_RENDERABLE_ENTITY, entity);

        // Die Luecke schliesst die letzte Figur: die Reihenfolge der Ablage
        // sagt nichts, gezeichnet wird nach Tiefe.
        map->entities[i] = map->entities[map->number_of_entities - 1];
        map->number_of_entities--;
        free(entity);
        return;
    }
}

///
/// Legt eine Figur fuer die Einheit "id" an und traegt sie bei (x, y) ein; NULL,
/// wenn kein Platz oder kein Speicher mehr da ist.
///
static RAD_IsoEntity_t* RAD_IsoMapCreateEntity(RAD_IsoMap_t *map, RAD_NetEntityId_t id, int32_t x, int32_t y)
{
    if(map->number_of_entities >= RAD_ISO_MAP_ENTITIES_MAX)
    {
        return NULL;
    }

    // calloc: alles andere gilt erst, wenn es jemand setzt (view/rendering/entity.h).
    RAD_IsoEntity_t *entity = calloc(1, sizeof(RAD_IsoEntity_t));
    if(entity == NULL)
    {
        return NULL;
    }

    entity->id = id;
    entity->x = (float)x;
    entity->y = (float)y;

    if(!RAD_IsoMapAddRenderable(map, (RAD_IsoRenderable_t){
            .type = RAD_ISO_RENDERABLE_ENTITY, .layer = RAD_ISO_LAYER_ENTITY, .entity = entity }))
    {
        free(entity);
        return NULL;
    }

    map->entities[map->number_of_entities++] = entity;
    return entity;
}

bool RAD_IsoMapStartEffect(RAD_IsoMap_t *map, RAD_IsoEffectSprite_t sprite, RAD_IsoFacing_t facing,
                           float x, float y, float start_ms)
{
    for(int32_t i = 0; i < RAD_ISO_MAP_EFFECTS_MAX; ++i)
    {
        if(!map->effects[i].active)
        {
            // Noch nicht in die Zeichenliste: erst wenn er laeuft
            // (RAD_IsoMapUpdateEffects).
            RAD_IsoEffectStart(&map->effects[i], sprite, facing, x, y, start_ms);
            return true;
        }
    }
    return false;
}

void RAD_IsoMapUpdateEffects(RAD_IsoMap_t *map, float now_ms)
{
    for(int32_t i = 0; i < RAD_ISO_MAP_EFFECTS_MAX; ++i)
    {
        RAD_IsoEffect_t *effect = &map->effects[i];
        if(!effect->active)
        {
            continue;
        }

        const bool running = RAD_IsoEffectUpdate(effect, now_ms);
        const bool shown = running && RAD_IsoEffectHasStarted(effect, now_ms);

        if(shown)
        {
            // Platz ist immer: die Zeichenliste haelt RAD_ISO_MAP_EFFECTS_MAX
            // Plaetze bereit (RAD_IsoMap_t.iso_objects). Steht er schon darin,
            // bleibt es dabei.
            RAD_IsoMapAddRenderable(map, (RAD_IsoRenderable_t){
                .type = RAD_ISO_RENDERABLE_EFFECT, .layer = RAD_ISO_LAYER_EFFECT, .effect = effect });
        }
        else if(!running)
        {
            RAD_IsoMapRemoveRenderable(map, RAD_ISO_RENDERABLE_EFFECT, effect);
        }
    }
}

bool RAD_IsoMapHasEffects(const RAD_IsoMap_t *map)
{
    for(int32_t i = 0; i < RAD_ISO_MAP_EFFECTS_MAX; ++i)
    {
        if(map->effects[i].active)
        {
            return true;
        }
    }
    return false;
}

float RAD_IsoMapImpactMs(const RAD_IsoMap_t *map, int32_t x, int32_t y, float fallback_ms)
{
    float impact_ms = fallback_ms;
    for(int32_t i = 0; i < RAD_ISO_MAP_EFFECTS_MAX; ++i)
    {
        const RAD_IsoEffect_t *effect = &map->effects[i];
        if(effect->active && ((int32_t)effect->x == x) && ((int32_t)effect->y == y)
           && (effect->start_ms > impact_ms))
        {
            impact_ms = effect->start_ms;
        }
    }
    return impact_ms;
}

void RAD_IsoMapApplyTile(RAD_IsoMap_t *map, const RAD_ClientTile_t *tile, int32_t player)
{
    const int32_t x = tile->x;
    const int32_t y = tile->y;

    if(!RAD_IsoMapInBounds(x, y))
    {
        printf("IsoMap: Feld (%d,%d) liegt ausserhalb des Rasters\n", (int)x, (int)y);
        return;
    }

    if(!map->data[y][x].present)
    {
        RAD_MapAddIsoObject(map, x, y);
        qsort(
            map->iso_objects, map->number_of_iso_objects, sizeof(RAD_IsoRenderable_t), RAD_MapDepthComparator
        );
    }

    // Bei jedem Aufruf und nicht nur beim Anlegen: auch ein Feld, das den Typ
    // wechselt, sieht danach aus wie sein neuer Typ (view/rendering/iso_tile.h).
    RAD_IsoTileApplyType(&map->data[y][x], map->decor[y][x], tile->type);
    RAD_IsoMapApplyDecor(map, x, y);

    // Ein Siegfeld zeichnet sich selbst anders (RAD_RenderIsoObject).
    map->data[y][x].victory = tile->victory;

    RAD_IsoMapSetEntity(map, x, y, tile->unit, player);
}

void RAD_IsoMapRemoveTile(RAD_IsoMap_t *map, uint32_t x, uint32_t y)
{
    if(!RAD_IsoMapInBounds((int32_t)x, (int32_t)y))
    {
        return;
    }

    if(map->data[y][x].present)
    {
        RAD_IsoMapRemoveObject(map, &map->data[y][x]);
    }
    RAD_IsoMapRemoveDecor(map, (int32_t)x, (int32_t)y);

    // Mit dem Feld geht die Figur darauf -- es sei denn, sie spielt noch etwas
    // ab (RAD_IsoEntity_t.busy).
    RAD_IsoEntity_t *standing = RAD_IsoMapEntityAt(map, (int32_t)x, (int32_t)y);
    if((standing != NULL) && !RAD_IsoEntityHoldsPosition(standing))
    {
        RAD_IsoMapRemoveEntity(map, standing->id);
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
/// Setzt die Einheit "unit" auf Feld (x, y) oder nimmt sie weg (NULL). Einheiten
/// Das Bild der Figur folgt dem Bild-Schluessel der Einheit (RAD_IsoUnitSpriteOf);
/// kommt eine andere Einheit aufs Feld, gilt ihre Blickrichtung als nicht gesetzt.
/// Ihr Ring ist "player" (view/rendering/entity.h).
///
static void RAD_IsoMapSetEntity(RAD_IsoMap_t *map, int32_t x, int32_t y, const RAD_ClientUnit_t *unit, int32_t player)
{
    if(unit == NULL)
    {
        // Das Feld ist leer. Eine Figur, die gerade etwas abspielt, bleibt
        // trotzdem stehen: sie zieht hier fort oder brennt noch
        // (RAD_IsoEntity_t.busy).
        RAD_IsoEntity_t *standing = RAD_IsoMapEntityAt(map, x, y);
        if((standing != NULL) && !RAD_IsoEntityHoldsPosition(standing))
        {
            RAD_IsoMapRemoveEntity(map, standing->id);
        }
        return;
    }

    RAD_IsoEntity_t *entity = RAD_IsoMapEntityOf(map, unit->id);
    const bool created = (entity == NULL);
    if(created)
    {
        entity = RAD_IsoMapCreateEntity(map, unit->id, x, y);
        if(entity == NULL)
        {
            return;
        }
        entity->facing = RAD_ISO_FACING_X_MINUS;
    }

    // Eine Figur, die gerade zieht, steht nicht da, wo das Feld sie meldet:
    // ihre Werte gehoeren dem, der sie abspielt (RAD_IsoEntity_t.busy).
    if(!RAD_IsoEntityHoldsPosition(entity))
    {
        entity->x = (float)x;
        entity->y = (float)y;
    }

    entity->sprite = RAD_IsoUnitSpriteOf(unit->sprite);
    entity->player = player;
}

///
/// Bringt die Zeichenliste auf den Stand der Dekorationen von Feld (x, y): was
/// ein Bild hat, steht darin, was keines hat, nicht.
///
static void RAD_IsoMapApplyDecor(RAD_IsoMap_t *map, int32_t x, int32_t y)
{
    for(int32_t i = 0; i < RAD_ISO_DECOR_PER_TILE; ++i)
    {
        RAD_IsoDecor_t *decor = &map->decor[y][x][i];
        if(decor->image != NULL)
        {
            // Platz ist immer: die Zeichenliste haelt RAD_ISO_DECOR_PER_TILE
            // Plaetze je Feld bereit (RAD_IsoMap_t.iso_objects).
            RAD_IsoMapAddRenderable(map, (RAD_IsoRenderable_t){
                .type = RAD_ISO_RENDERABLE_DECOR, .layer = RAD_ISO_LAYER_DECOR, .decor = decor
            });
        }
        else
        {
            RAD_IsoMapRemoveRenderable(map, RAD_ISO_RENDERABLE_DECOR, decor);
        }
    }
}

///
/// Nimmt die Dekorationen von Feld (x, y) aus der Zeichenliste und loescht sie.
///
static void RAD_IsoMapRemoveDecor(RAD_IsoMap_t *map, int32_t x, int32_t y)
{
    for(int32_t i = 0; i < RAD_ISO_DECOR_PER_TILE; ++i)
    {
        RAD_IsoDecor_t *decor = &map->decor[y][x][i];
        RAD_IsoMapRemoveRenderable(map, RAD_ISO_RENDERABLE_DECOR, decor);
        *decor = (RAD_IsoDecor_t){ .image = NULL };
    }
}

static bool RAD_IsoMapInBounds(int32_t x, int32_t y)
{
    return (x >= 0) && (x < RAD_ISO_MAP_SIZE) && (y >= 0) && (y < RAD_ISO_MAP_SIZE);
}

