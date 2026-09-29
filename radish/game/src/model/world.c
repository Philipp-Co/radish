#include <radish/game/model/world/world.h>
#include <stddef.h>
#include <stdio.h>

static RAD_EntityId_t RAD_WorldFindFreeEntitySlot(RAD_World_t *world);
static void RAD_WorldPublishTileTransition(RAD_World_t *world, RAD_TileType_t previous_type, int32_t previous_z, const RAD_Tile_t *tile);

RAD_World_t RAD_CreateWorld(RAD_EventManager_t *event_manager)
{
    RAD_World_t world;
    world.width = RAD_WORLD_WIDTH;
    world.height = RAD_WORLD_HEIGHT;
    world.number_of_entities = 0;
    world.event_manager = event_manager;
    return world;
}

void RAD_InitWorld(RAD_World_t *world)
{
    RAD_ResetWorld(world);

    // Vorher war nichts, jetzt ist Boden: fuer einen Abonnenten ist das VOID -> X,
    // also added -- derselbe Uebergang wie in RAD_WorldAddTile.
    for(int32_t y=0;y < world->height; ++y)
    {
        for(int32_t x=0;x < world->width; ++x)
        {
            RAD_EventManagerPublishTileAddedToGameEvent(world->event_manager, &(world->tiles[y][x]));
        }
    }
}

void RAD_ResetWorld(RAD_World_t *world)
{
    RAD_ResetWorldToSize(world, world->width, world->height);
}

bool RAD_ResetWorldToSize(RAD_World_t *world, int32_t width, int32_t height)
{
    if(width < 1 || width > RAD_WORLD_WIDTH || height < 1 || height > RAD_WORLD_HEIGHT)
    {
        return false;
    }

    world->width = width;
    world->height = height;

    // Der ganze Speicher, nicht nur die Welt: auch ein Feld daneben traegt sein x
    // und y, denn RAD_WorldPublishTileChanges meldet es, wenn es beim Schrumpfen
    // wegfaellt (world.h).
    for(int32_t y=0;y < RAD_WORLD_HEIGHT; ++y)
    {
        for(int32_t x=0;x < RAD_WORLD_WIDTH; ++x)
        {
            const bool inside = (x < width) && (y < height);
            world->tiles[y][x] = (RAD_Tile_t){
                .x = x,
                .y = y,
                .z = 0,
                .type = inside ? RAD_TILE_TYPE_GROUND : RAD_TILE_TYPE_VOID,
                .entity = RAD_ENTITY_NONE
            };
        }
    }

    for(int32_t i=0;i < RAD_MAX_ENTITIES; ++i)
    {
        world->entities[i] = (RAD_Entity_t){
            .id = RAD_ENTITY_NONE,
            .type = RAD_ENTITY_TYPE_NONE,
            .owner = RAD_USER_NONE,
            .x = 0,
            .y = 0
        };
    }

    world->number_of_entities = 0;

    return true;
}

bool RAD_WorldInBounds(const RAD_World_t *world, int32_t x, int32_t y)
{
    return (x >= 0) && (x < world->width) && (y >= 0) && (y < world->height);
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

bool RAD_WorldAddTile(RAD_World_t *world, int32_t x, int32_t y, int32_t z, RAD_TileType_t type)
{
    if(type == RAD_TILE_TYPE_VOID)
    {
        // Kein Gelaende hinzustellen ist Gelaende wegnehmen -- dasselbe Ergebnis,
        // also derselbe Weg. Das z faellt dabei weg: was nicht da ist, hat keine
        // Hoehe zu melden.
        return RAD_WorldRemoveTile(world, x, y);
    }

    RAD_Tile_t *tile = RAD_WorldTileAt(world, x, y);
    if(tile == NULL)
    {
        return false;
    }

    const RAD_TileType_t previous = tile->type;
    if((previous == type) && (tile->z == z))
    {
        // Derselbe Stand: geschrieben wird nichts und gemeldet auch nichts. Ein
        // Ereignis ohne Aenderung waere eine Nachricht ohne Inhalt.
        return true;
    }

    const int32_t previous_z = tile->z;
    tile->type = type;
    tile->z = z;

    RAD_WorldPublishTileTransition(world, previous, previous_z, tile);

    return true;
}

bool RAD_WorldRemoveTile(RAD_World_t *world, int32_t x, int32_t y)
{
    RAD_Tile_t *tile = RAD_WorldTileAt(world, x, y);
    if(tile == NULL)
    {
        return false;
    }

    if(tile->type == RAD_TILE_TYPE_VOID)
    {
        // Schon leer, und zwar auch unter einer Figur: die Pruefung darunter
        // schuetzt eine Aenderung, nicht das Feld.
        return true;
    }

    // Erst pruefen, dann schreiben -- dieselbe Reihenfolge wie in
    // RAD_WorldMoveEntity. Eine Figur haelt ihr Gelaende (world.h).
    if(tile->entity != RAD_ENTITY_NONE)
    {
        return false;
    }

    // Nur der Typ. x, y, z und die Entitaet bleiben stehen: weggenommen wird das
    // Gelaende und nicht das Feld.
    const RAD_TileType_t previous = tile->type;
    tile->type = RAD_TILE_TYPE_VOID;

    RAD_WorldPublishTileTransition(world, previous, tile->z, tile);

    return true;
}

void RAD_WorldPublishTileChanges(
    RAD_World_t *world,
    const RAD_Tile_t previous[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH],
    int32_t previous_width,
    int32_t previous_height)
{
    const int32_t width = (world->width > previous_width) ? world->width : previous_width;
    const int32_t height = (world->height > previous_height) ? world->height : previous_height;

    for(int32_t y=0;y < height; ++y)
    {
        for(int32_t x=0;x < width; ++x)
        {
            // Ausserhalb der alten Welt war nichts. Ausserhalb der neuen steht nach
            // RAD_ResetWorldToSize schon VOID im Speicher, das Feld selbst sagt es.
            const bool was_inside = (x < previous_width) && (y < previous_height);
            const RAD_TileType_t previous_type = was_inside ? previous[y][x].type : RAD_TILE_TYPE_VOID;

            RAD_WorldPublishTileTransition(world, previous_type, previous[y][x].z, &world->tiles[y][x]);
        }
    }
}

///
/// Die Tabelle aus world.h, an einer Stelle: RAD_WorldAddTile, RAD_WorldRemoveTile
/// und RAD_WorldPublishTileChanges melden alle ueber sie, damit ein Uebergang nicht
/// je nach Weg anders heisst. Die Hoehe zaehlt nur zwischen zwei Feldern mit
/// Gelaende -- was nicht da ist, hat keine Hoehe zu melden.
///
static void RAD_WorldPublishTileTransition(RAD_World_t *world, RAD_TileType_t previous_type, int32_t previous_z, const RAD_Tile_t *tile)
{
    const bool was_void = (previous_type == RAD_TILE_TYPE_VOID);
    const bool is_void = (tile->type == RAD_TILE_TYPE_VOID);

    if(was_void && is_void)
    {
        return;
    }

    if(was_void)
    {
        RAD_EventManagerPublishTileAddedToGameEvent(world->event_manager, tile);
    }
    else if(is_void)
    {
        RAD_EventManagerPublishTileRemovedFromGameEvent(world->event_manager, tile);
    }
    else if((previous_type != tile->type) || (previous_z != tile->z))
    {
        RAD_EventManagerPublishTileStateChangeEvent(world->event_manager, tile);
    }
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
    if((tile == NULL) || (tile->entity != RAD_ENTITY_NONE))
    {
        return RAD_ENTITY_NONE;
    }

    // Der ganze Platz wird geschrieben, nicht nur die Felder, die diese Funktion
    // kennt: eine neue Figur faengt herrenlos an, gleich wem die vorige in diesem
    // Slot gehoert hat. Wer sie zuordnen will, tut das danach.
    world->entities[id] = (RAD_Entity_t){
        .id = id,
        .type = type,
        .owner = RAD_USER_NONE,
        .x = x,
        .y = y
    };
    tile->entity = id;
    world->number_of_entities++;

    RAD_EventManagerPublishEntitySpawned(world->event_manager, &world->entities[id], x, y);

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

    // Woher die Figur kommt, muss vor dem Schreiben festgehalten werden: gleich
    // steht in entity->x/y das Ziel, und das Ereignis unten braucht beides.
    const int16_t from_x = entity->x;
    const int16_t from_y = entity->y;

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

    // Ein Zug ueber ein Feld sind zwei Felder: das verlassene und das betretene.
    // Beide gehoeren in den Pfad -- ein Abonnent hat eine Figur umzuhaengen und
    // braucht dafuer beide Enden (path.h). Aus entity->x/y liesse sich das nicht
    // mehr holen: dort steht seit drei Zeilen das Ziel.
    const RAD_EntityPath_t path = {
        .steps_to = {
            { .x = from_x,     .y = from_y     },
            { .x = (int16_t)x, .y = (int16_t)y }
        },
        .number_of_steps = 2
    };

    RAD_EventManagerPublishEntityMoved(world->event_manager, entity, &path, 0);

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

    RAD_EventManagerPublishEntityDestroyed(world->event_manager, entity, entity->x, entity->y);

    // Der Slot wird nur als frei markiert, nicht herausgeschnitten: die Id ist
    // der Array-Index, ein Zusammenschieben wuerde alle anderen Ids entwerten.
    entity->id = RAD_ENTITY_NONE;
    entity->type = RAD_ENTITY_TYPE_NONE;

    // Der Besitz faellt mit der Figur weg. RAD_WorldSpawnEntityWithId setzt ihn
    // ohnehin neu; hier steht er trotzdem, damit ein freier Platz nie einen
    // Besitzer traegt -- darauf prueft RAD_WorldIsConsistent.
    entity->owner = RAD_USER_NONE;

    world->number_of_entities--;
}

RAD_UserId_t RAD_WorldEntityOwner(const RAD_World_t *world, RAD_EntityId_t id)
{
    if(id < 0 || id >= RAD_MAX_ENTITIES)
    {
        return RAD_USER_NONE;
    }

    const RAD_Entity_t *entity = &world->entities[id];
    if(entity->id == RAD_ENTITY_NONE)
    {
        return RAD_USER_NONE;
    }

    return entity->owner;
}

bool RAD_WorldSetEntityOwner(RAD_World_t *world, RAD_EntityId_t id, RAD_UserId_t owner)
{
    RAD_Entity_t *entity = RAD_WorldEntityById(world, id);
    if(entity == NULL)
    {
        return false;
    }

    entity->owner = owner;
    return true;
}

bool RAD_WorldIsConsistent(const RAD_World_t *world)
{
    int32_t live_entities = 0;

    for(RAD_EntityId_t i=0;i < RAD_MAX_ENTITIES; ++i)
    {
        const RAD_Entity_t *entity = &world->entities[i];
        if(entity->id == RAD_ENTITY_NONE)
        {
            // Ein freier Platz gehoert niemandem. Traegt er einen Besitzer, ist
            // eine Figur entfernt worden, ohne ihn zu raeumen -- und die naechste
            // in diesem Slot wuerde ihn erben.
            if(entity->owner != RAD_USER_NONE)
            {
                return false;
            }
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

    if(world->width < 1 || world->width > RAD_WORLD_WIDTH || world->height < 1 || world->height > RAD_WORLD_HEIGHT)
    {
        return false;
    }

    for(int32_t y=0;y < world->height; ++y)
    {
        for(int32_t x=0;x < world->width; ++x)
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
