#include <radish/model/world.h>

#include <stddef.h>
#include <string.h>

typedef void (*RAD_ClientWorldTileCreated_t)(void *user_argument, const RAD_ClientWorld_t *world, const RAD_ClientTile_t *tile);
typedef void (*RAD_ClientWorldReserveCreated_t)(void *user_argument, const RAD_ClientWorld_t *world, const RAD_ClientReserve_t *reserve);
typedef void (*RAD_ClientWorldSizeChanged_t)(void *user_argument, const RAD_ClientWorld_t *world);

static bool RAD_ClientWorldInBounds(int32_t x, int32_t y)
{
    return (x >= 0) && (x < RAD_ISO_MAP_SIZE) && (y >= 0) && (y < RAD_ISO_MAP_SIZE);
}

static RAD_ClientUnit_t* RAD_ClientWorldFieldUnit(RAD_ClientWorld_t *world, RAD_ClientUnitId_t id);
static RAD_ClientReserve_t* RAD_ClientWorldReserveContaining(RAD_ClientWorld_t *world, RAD_ClientUnitId_t id);

//
// Die drei Ereignisse haben je eine eigene Signatur, also je eine eigene
// Meldefunktion. Alle laufen ueber eine Kopie der Liste: ein Beobachter darf
// sich im Callback an- und abmelden.
//

static void RAD_ClientWorldNotifyTileCreated(const RAD_ClientWorld_t *world, const RAD_ClientTile_t *tile)
{
    const RAD_ModelObservable_t observable = world->observable;

    for(uint32_t i = 0; i < observable.number_of_observers; ++i)
    {
        const RAD_ModelObserver_t *observer = &observable.observers[i];
        const RAD_ClientWorldTileCreated_t callback = (RAD_ClientWorldTileCreated_t)observer->callbacks[RAD_CLIENT_WORLD_EVENT_TILE_CREATED];
        if(callback != NULL)
        {
            callback(observer->user_argument, world, tile);
        }
    }
}

static void RAD_ClientWorldNotifyReserveCreated(const RAD_ClientWorld_t *world, const RAD_ClientReserve_t *reserve)
{
    const RAD_ModelObservable_t observable = world->observable;

    for(uint32_t i = 0; i < observable.number_of_observers; ++i)
    {
        const RAD_ModelObserver_t *observer = &observable.observers[i];
        const RAD_ClientWorldReserveCreated_t callback = (RAD_ClientWorldReserveCreated_t)observer->callbacks[RAD_CLIENT_WORLD_EVENT_RESERVE_CREATED];
        if(callback != NULL)
        {
            callback(observer->user_argument, world, reserve);
        }
    }
}

static void RAD_ClientWorldNotifySizeChanged(const RAD_ClientWorld_t *world)
{
    const RAD_ModelObservable_t observable = world->observable;

    for(uint32_t i = 0; i < observable.number_of_observers; ++i)
    {
        const RAD_ModelObserver_t *observer = &observable.observers[i];
        const RAD_ClientWorldSizeChanged_t callback = (RAD_ClientWorldSizeChanged_t)observer->callbacks[RAD_CLIENT_WORLD_EVENT_SIZE_CHANGED];
        if(callback != NULL)
        {
            callback(observer->user_argument, world);
        }
    }
}

void RAD_ClientWorldInit(RAD_ClientWorld_t *world)
{
    memset(world, 0, sizeof(*world));
}

void RAD_ClientWorldSetSize(RAD_ClientWorld_t *world, uint32_t width, uint32_t height)
{
    if((world->width == width) && (world->height == height))
    {
        return;
    }

    world->width = width;
    world->height = height;
    RAD_ClientWorldNotifySizeChanged(world);
}

void RAD_ClientWorldApplyTile(RAD_ClientWorld_t *world,
                              RAD_ClientPosition_t position,
                              RAD_ClientTileType_t type,
                              RAD_ClientUnitId_t unit)
{
    if(!RAD_ClientWorldInBounds(position.x, position.y))
    {
        return;
    }

    RAD_ClientTile_t *target = &world->tiles[position.y][position.x];
    const bool created = !world->known[position.y][position.x];

    // Ein Feld, das neu ist, hat keine Beobachter -- RAD_ClientWorldRemoveTile hat
    // sie abgemeldet --, sein "changed" erreicht also niemanden.
    // Auf dem Feld steht nur eine Einheit, die die Welt schon kennt (world.h).
    world->known[position.y][position.x] = true;
    RAD_ClientTileSet(target, position.x, position.y, type, RAD_ClientWorldFieldUnit(world, unit));

    if(created)
    {
        RAD_ClientWorldNotifyTileCreated(world, target);
    }
}

void RAD_ClientWorldRemoveTile(RAD_ClientWorld_t *world, uint32_t x, uint32_t y)
{
    if(!RAD_ClientWorldInBounds((int32_t)x, (int32_t)y) || !world->known[y][x])
    {
        return;
    }

    world->known[y][x] = false;
    RAD_ClientTileRemove(&world->tiles[y][x]);
}

const RAD_ClientTile_t* RAD_ClientWorldTileAt(const RAD_ClientWorld_t *world, int32_t x, int32_t y)
{
    if(!RAD_ClientWorldInBounds(x, y) || !world->known[y][x])
    {
        return NULL;
    }

    return &world->tiles[y][x];
}

void RAD_ClientWorldMoveEntity(RAD_ClientWorld_t *world, RAD_ClientUnitId_t unit_id, RAD_ClientPosition_t from, RAD_ClientPosition_t to)
{
    if(!RAD_ClientWorldInBounds(from.x, from.y) || !RAD_ClientWorldInBounds(to.x, to.y))
    {
        return;
    }

    RAD_ClientUnit_t *unit = world->tiles[from.y][from.x].unit;
    if((unit == NULL) || (unit->id != unit_id))
    {
        return;
    }

    RAD_ClientTileSetUnit(&world->tiles[from.y][from.x], NULL);
    RAD_ClientTileSetUnit(&world->tiles[to.y][to.x], unit);
}

static RAD_ClientReserve_t* RAD_ClientWorldReserveOf(RAD_ClientWorld_t *world, RAD_ClientPlayerId_t owner)
{
    for(size_t i = 0; i < RAD_CLIENT_WORLD_PLAYERS; ++i)
    {
        if(world->reserves[i].owner == owner)
        {
            return &world->reserves[i];
        }
    }
    return NULL;
}

bool RAD_ClientWorldAddUnit(RAD_ClientWorld_t *world, const RAD_ClientUnit_t *unit)
{
    if(unit->owner == RAD_CLIENT_PLAYER_NONE)
    {
        return false;
    }

    RAD_ClientUnit_t *stored = RAD_ClientUnitRepositoryFind(&world->units, unit->id);
    if(stored == NULL)
    {
        stored = RAD_ClientUnitRepositoryCreate(&world->units, unit->id, unit->owner, unit->name);
        if(stored == NULL)
        {
            return false;
        }
    }

    // Wie in RAD_ClientWorldAddReserveUnit: eine neue Einheit hat noch keine
    // Beobachter, eine bekannte meldet so, dass sie ersetzt wurde.
    RAD_ClientUnitAssign(stored, unit);
    return true;
}

bool RAD_ClientWorldAddReserveUnit(RAD_ClientWorld_t *world, const RAD_ClientUnit_t *unit)
{
    if(unit->owner == RAD_CLIENT_PLAYER_NONE)
    {
        return false;
    }

    // Erst die eigene Reserve des Besitzers, sonst die erste freie.
    RAD_ClientReserve_t *reserve = RAD_ClientWorldReserveOf(world, unit->owner);
    const bool reserve_is_new = (reserve == NULL);
    if(reserve_is_new)
    {
        reserve = RAD_ClientWorldReserveOf(world, RAD_CLIENT_PLAYER_NONE);
        if(reserve == NULL)
        {
            return false;
        }
    }

    // Erst pruefen, dann aendern: scheitert es, bleibt alles, wie es war.
    if(!RAD_ClientReserveContains(reserve, unit->id)
       && (RAD_ClientReserveNumberOfUnits(reserve) >= RAD_CLIENT_RESERVE_UNITS_MAX))
    {
        return false;
    }

    RAD_ClientUnit_t *stored = RAD_ClientUnitRepositoryFind(&world->units, unit->id);
    if(stored == NULL)
    {
        stored = RAD_ClientUnitRepositoryCreate(&world->units, unit->id, unit->owner, unit->name);
        if(stored == NULL)
        {
            return false;
        }
    }

    if(reserve_is_new)
    {
        // Eine freie Reserve ist leer, die Einheit passt also hinein.
        reserve->owner = unit->owner;
        RAD_ClientWorldNotifyReserveCreated(world, reserve);
    }

    // Eine neue Einheit hat noch keine Beobachter, ihr "changed" erreicht
    // niemanden; eine bekannte meldet so, dass sie ersetzt wurde.
    RAD_ClientUnitAssign(stored, unit);
    return RAD_ClientReserveAddUnit(reserve, unit->id);
}

bool RAD_ClientWorldRemoveReserveUnit(RAD_ClientWorld_t *world, RAD_ClientUnitId_t id)
{
    RAD_ClientReserve_t *reserve = RAD_ClientWorldReserveContaining(world, id);
    if(reserve == NULL)
    {
        return false;
    }

    RAD_ClientReserveRemoveUnit(reserve, id);
    RAD_ClientUnitRepositoryRemove(&world->units, id);
    return true;
}

bool RAD_ClientWorldDeployUnit(RAD_ClientWorld_t *world, RAD_ClientUnitId_t id, RAD_ClientPosition_t position)
{
    if(!RAD_ClientWorldInBounds(position.x, position.y) || !world->known[position.y][position.x])
    {
        return false;
    }
    RAD_ClientTile_t *tile = &world->tiles[position.y][position.x];
    if(tile->unit != NULL)
    {
        return false;
    }

    RAD_ClientReserve_t *reserve = RAD_ClientWorldReserveContaining(world, id);
    RAD_ClientUnit_t *unit = RAD_ClientUnitRepositoryFind(&world->units, id);
    if((reserve == NULL) || (unit == NULL))
    {
        return false;
    }

    // Die Einheit bleibt, wo sie ist; nur die Reserve nennt sie nicht mehr.
    RAD_ClientReserveRemoveUnit(reserve, id);
    RAD_ClientTileSetUnit(tile, unit);
    return true;
}

const RAD_ClientReserve_t* RAD_ClientWorldReserve(const RAD_ClientWorld_t *world, RAD_ClientPlayerId_t owner)
{
    if(owner == RAD_CLIENT_PLAYER_NONE)
    {
        return NULL;
    }

    for(size_t i = 0; i < RAD_CLIENT_WORLD_PLAYERS; ++i)
    {
        if(world->reserves[i].owner == owner)
        {
            return &world->reserves[i];
        }
    }
    return NULL;
}

void RAD_ClientWorldSetCurrentPlayer(RAD_ClientWorld_t *world, RAD_ClientPlayerId_t player)
{
    world->current_player = player;
}

RAD_ClientPlayerId_t RAD_ClientWorldCurrentPlayer(const RAD_ClientWorld_t *world)
{
    return world->current_player;
}

const RAD_ClientUnitRepository_t* RAD_ClientWorldUnits(const RAD_ClientWorld_t *world)
{
    return &world->units;
}

bool RAD_ClientWorldSubscribe(const RAD_ClientWorld_t *world, RAD_ClientWorldObserver_t observer)
{
    const RAD_ModelObserver_t generic = {
        .user_argument = observer.user_argument,
        .callbacks = {
            [RAD_CLIENT_WORLD_EVENT_TILE_CREATED] = (RAD_ModelCallback_t)observer.tile_created,
            [RAD_CLIENT_WORLD_EVENT_RESERVE_CREATED] = (RAD_ModelCallback_t)observer.reserve_created,
            [RAD_CLIENT_WORLD_EVENT_SIZE_CHANGED] = (RAD_ModelCallback_t)observer.size_changed
        }
    };
    return RAD_ModelObservableSubscribe(RAD_ModelObservableOf(&world->observable), &generic);
}

bool RAD_ClientWorldUnsubscribe(const RAD_ClientWorld_t *world, const void *user_argument)
{
    return RAD_ModelObservableUnsubscribe(RAD_ModelObservableOf(&world->observable), user_argument);
}

///
/// Die Reserve, in der "id" steht, oder NULL.
///
static RAD_ClientReserve_t* RAD_ClientWorldReserveContaining(RAD_ClientWorld_t *world, RAD_ClientUnitId_t id)
{
    for(size_t i = 0; i < RAD_CLIENT_WORLD_PLAYERS; ++i)
    {
        if(RAD_ClientReserveContains(&world->reserves[i], id))
        {
            return &world->reserves[i];
        }
    }
    return NULL;
}

///
/// Die Einheit "id" auf dem Feld, oder NULL, wenn keine so heisst --
/// RAD_CLIENT_UNIT_ID_NONE eingeschlossen. Auf dem Feld steht, was die Welt kennt
/// und keine Reserve nennt.
///
static RAD_ClientUnit_t* RAD_ClientWorldFieldUnit(RAD_ClientWorld_t *world, RAD_ClientUnitId_t id)
{
    if(RAD_ClientWorldReserveContaining(world, id) != NULL)
    {
        return NULL;
    }
    return RAD_ClientUnitRepositoryFind(&world->units, id);
}
