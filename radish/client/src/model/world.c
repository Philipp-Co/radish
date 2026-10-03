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

void RAD_ClientWorldApplyTile(RAD_ClientWorld_t *world, const RAD_NetTile_t *tile)
{
    if(!RAD_ClientWorldInBounds((int32_t)tile->x, (int32_t)tile->y))
    {
        return;
    }

    RAD_ClientTile_t *target = &world->tiles[tile->y][tile->x];
    const bool created = !world->known[tile->y][tile->x];

    // Ein Feld, das neu ist, hat keine Beobachter -- RAD_ClientWorldRemoveTile hat
    // sie abgemeldet --, sein "changed" erreicht also niemanden.
    // Einheiten auf dem Feld haelt die Welt noch nicht (world.h).
    world->known[tile->y][tile->x] = true;
    RAD_ClientTileFromNet(target, tile, NULL);

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

void RAD_ClientWorldMoveEntity(RAD_ClientWorld_t *world, RAD_NetEntityId_t entity, const RAD_NetPath_t *path)
{
    // Unter zwei Feldern ist kein Weg beschrieben (net_types.h).
    if((path == NULL) || (path->number_of_steps < 2))
    {
        return;
    }

    const RAD_NetPosition_t from = path->steps_to[0];
    const RAD_NetPosition_t to = path->steps_to[path->number_of_steps - 1];

    if(!RAD_ClientWorldInBounds(from.x, from.y) || !RAD_ClientWorldInBounds(to.x, to.y))
    {
        return;
    }

    RAD_ClientUnit_t *unit = world->tiles[from.y][from.x].unit;
    if((unit == NULL) || (unit->id != entity))
    {
        return;
    }

    RAD_ClientTileSetUnit(&world->tiles[from.y][from.x], NULL);
    RAD_ClientTileSetUnit(&world->tiles[to.y][to.x], unit);
}

static RAD_ClientReserve_t* RAD_ClientWorldReserveOf(RAD_ClientWorld_t *world, RAD_NetUserId_t owner)
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

bool RAD_ClientWorldAddReserveUnit(RAD_ClientWorld_t *world, const RAD_ClientUnit_t *unit)
{
    if(unit->owner == RAD_NET_USER_NONE)
    {
        return false;
    }

    // Erst die eigene Reserve des Besitzers, sonst die erste freie.
    RAD_ClientReserve_t *reserve = RAD_ClientWorldReserveOf(world, unit->owner);
    if(reserve == NULL)
    {
        reserve = RAD_ClientWorldReserveOf(world, RAD_NET_USER_NONE);
        if(reserve == NULL)
        {
            return false;
        }

        // Eine freie Reserve ist leer, die Einheit passt also hinein.
        reserve->owner = unit->owner;
        RAD_ClientWorldNotifyReserveCreated(world, reserve);
    }

    return RAD_ClientReserveAddUnit(reserve, unit);
}

const RAD_ClientReserve_t* RAD_ClientWorldReserve(const RAD_ClientWorld_t *world, RAD_NetUserId_t owner)
{
    if(owner == RAD_NET_USER_NONE)
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
