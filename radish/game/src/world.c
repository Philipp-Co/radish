#include <radish/game/world.h>
#include <stddef.h>

static RAD_EntityId_t RAD_WorldFindFreeEntitySlot(RAD_World_t *world);

RAD_World_t RAD_CreateWorld(RAD_EventManager_t *event_manager)
{
    RAD_World_t world;
    world.number_of_entities = 0;
    world.event_manager = event_manager;
    return world;
}

void RAD_InitWorld(RAD_World_t *world)
{
    for(int32_t y=0;y < RAD_WORLD_HEIGHT; ++y)
    {
        for(int32_t x=0;x < RAD_WORLD_WIDTH; ++x)
        {
            world->tiles[y][x] = (RAD_Tile_t){
                .x = x,
                .y = y,
                .z = 0,
                .type = RAD_TILE_TYPE_GROUND,
                .entity = RAD_ENTITY_NONE
            };
            RAD_EventManagerPublishTileAddedToGameEvent(world->event_manager, &(world->tiles[y][x]));
        }
    }

    for(int32_t i=0;i < RAD_MAX_ENTITIES; ++i)
    {
        world->entities[i] = (RAD_Entity_t){
            .id = RAD_ENTITY_NONE,
            .type = RAD_ENTITY_TYPE_NONE,
            .x = 0,
            .y = 0
        };
    }

    world->number_of_entities = 0;
}

bool RAD_WorldInBounds(const RAD_World_t *world, int32_t x, int32_t y)
{
    (void)world;
    return x >= 0 && x < RAD_WORLD_WIDTH && y >= 0 && y < RAD_WORLD_HEIGHT;
}

RAD_Tile_t* RAD_WorldTileAt(RAD_World_t *world, int32_t x, int32_t y)
{
    if(!RAD_WorldInBounds(world, x, y))
    {
        return NULL;
    }
    return &world->tiles[y][x];
}

RAD_Entity_t* RAD_WorldEntityById(RAD_World_t *world, RAD_EntityId_t id)
{
    if(id < 0 || id >= RAD_MAX_ENTITIES)
    {
        return NULL;
    }

    RAD_Entity_t *entity = &world->entities[id];
    if(entity->id == RAD_ENTITY_NONE)
    {
        return NULL;
    }
    return entity;
}

RAD_Entity_t* RAD_WorldEntityAt(RAD_World_t *world, int32_t x, int32_t y)
{
    RAD_Tile_t *tile = RAD_WorldTileAt(world, x, y);
    if(tile == NULL)
    {
        return NULL;
    }
    return RAD_WorldEntityById(world, tile->entity);
}

RAD_EntityId_t RAD_WorldSpawnEntity(RAD_World_t *world, RAD_EntityType_t type, int32_t x, int32_t y)
{
    RAD_EntityId_t id = RAD_WorldFindFreeEntitySlot(world);
    if(id == RAD_ENTITY_NONE)
    {
        return RAD_ENTITY_NONE;
    }
    return RAD_WorldSpawnEntityWithId(world, id, type, x, y);
}

RAD_EntityId_t RAD_WorldSpawnEntityWithId(RAD_World_t *world, RAD_EntityId_t id, RAD_EntityType_t type, int32_t x, int32_t y)
{
    if(id < 0 || id >= RAD_MAX_ENTITIES || world->entities[id].id != RAD_ENTITY_NONE)
    {
        return RAD_ENTITY_NONE;
    }

    RAD_Tile_t *tile = RAD_WorldTileAt(world, x, y);
    if(tile == NULL || tile->entity != RAD_ENTITY_NONE)
    {
        return RAD_ENTITY_NONE;
    }

    world->entities[id] = (RAD_Entity_t){
        .id = id,
        .type = type,
        .x = x,
        .y = y
    };
    tile->entity = id;
    world->number_of_entities++;

    return id;
}

bool RAD_WorldMoveEntity(RAD_World_t *world, RAD_EntityId_t id, int32_t x, int32_t y)
{
    RAD_Entity_t *entity = RAD_WorldEntityById(world, id);
    if(entity == NULL)
    {
        return false;
    }

    if(entity->x == x && entity->y == y)
    {
        return true;
    }

    RAD_Tile_t *target = RAD_WorldTileAt(world, x, y);
    if(target == NULL || target->entity != RAD_ENTITY_NONE)
    {
        return false;
    }

    // Ab hier ist der Zug gueltig; erst jetzt wird geschrieben, damit ein
    // abgelehnter Zug die Welt garantiert unveraendert laesst.
    RAD_Tile_t *source = RAD_WorldTileAt(world, entity->x, entity->y);
    if(source != NULL)
    {
        source->entity = RAD_ENTITY_NONE;
    }

    target->entity = id;
    entity->x = x;
    entity->y = y;

    return true;
}

void RAD_WorldRemoveEntity(RAD_World_t *world, RAD_EntityId_t id)
{
    RAD_Entity_t *entity = RAD_WorldEntityById(world, id);
    if(entity == NULL)
    {
        return;
    }

    RAD_Tile_t *tile = RAD_WorldTileAt(world, entity->x, entity->y);
    if(tile != NULL)
    {
        tile->entity = RAD_ENTITY_NONE;
    }

    // Der Slot wird nur als frei markiert, nicht herausgeschnitten: die Id ist
    // der Array-Index, ein Zusammenschieben wuerde alle anderen Ids entwerten.
    entity->id = RAD_ENTITY_NONE;
    entity->type = RAD_ENTITY_TYPE_NONE;
    world->number_of_entities--;
}

bool RAD_WorldIsConsistent(const RAD_World_t *world)
{
    int32_t live_entities = 0;

    for(RAD_EntityId_t i=0;i < RAD_MAX_ENTITIES; ++i)
    {
        const RAD_Entity_t *entity = &world->entities[i];
        if(entity->id == RAD_ENTITY_NONE)
        {
            continue;
        }
        live_entities++;

        // Die Id ist der Slot-Index; alles andere waere ein verschobener Pool.
        if(entity->id != i)
        {
            return false;
        }
        if(!RAD_WorldInBounds(world, entity->x, entity->y))
        {
            return false;
        }
        // Das Tile unter der Entitaet muss auf sie zurueckzeigen.
        if(world->tiles[entity->y][entity->x].entity != entity->id)
        {
            return false;
        }
    }

    if(live_entities != world->number_of_entities)
    {
        return false;
    }

    for(int32_t y=0;y < RAD_WORLD_HEIGHT; ++y)
    {
        for(int32_t x=0;x < RAD_WORLD_WIDTH; ++x)
        {
            const RAD_Tile_t *tile = &world->tiles[y][x];
            if(tile->x != x || tile->y != y)
            {
                return false;
            }
            if(tile->entity == RAD_ENTITY_NONE)
            {
                continue;
            }
            if(tile->entity < 0 || tile->entity >= RAD_MAX_ENTITIES)
            {
                return false;
            }

            // Und die Entitaet muss genau hier stehen.
            const RAD_Entity_t *entity = &world->entities[tile->entity];
            if(entity->id != tile->entity || entity->x != x || entity->y != y)
            {
                return false;
            }
        }
    }

    return true;
}

static RAD_EntityId_t RAD_WorldFindFreeEntitySlot(RAD_World_t *world)
{
    for(RAD_EntityId_t i=0;i < RAD_MAX_ENTITIES; ++i)
    {
        if(world->entities[i].id == RAD_ENTITY_NONE)
        {
            return i;
        }
    }
    return RAD_ENTITY_NONE;
}
