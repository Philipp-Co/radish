#include <radish/model/tile.h>

#include <stddef.h>

typedef void (*RAD_ClientTileCallback_t)(void *user_argument, const RAD_ClientTile_t *tile);

static void RAD_ClientTileNotify(const RAD_ClientTile_t *tile, RAD_ClientTileEvent_t event)
{
    // Eine Kopie: ein Beobachter darf sich im Callback an- und abmelden.
    const RAD_ModelObservable_t observable = tile->observable;

    for(uint32_t i = 0; i < observable.number_of_observers; ++i)
    {
        const RAD_ModelObserver_t *observer = &observable.observers[i];
        const RAD_ClientTileCallback_t callback = (RAD_ClientTileCallback_t)observer->callbacks[event];
        if(callback != NULL)
        {
            callback(observer->user_argument, tile);
        }
    }
}

void RAD_ClientTileSet(RAD_ClientTile_t *tile, int32_t x, int32_t y, RAD_ClientTileType_t type, RAD_ClientUnit_t *unit)
{
    tile->x = x;
    tile->y = y;
    tile->type = type;
    tile->unit = unit;

    RAD_ClientTileNotify(tile, RAD_CLIENT_TILE_EVENT_CHANGED);
}

void RAD_ClientTileSetUnit(RAD_ClientTile_t *tile, RAD_ClientUnit_t *unit)
{
    if(tile->unit == unit)
    {
        return;
    }

    tile->unit = unit;
    RAD_ClientTileNotify(tile, RAD_CLIENT_TILE_EVENT_CHANGED);
}

void RAD_ClientTileRemove(RAD_ClientTile_t *tile)
{
    RAD_ClientTileNotify(tile, RAD_CLIENT_TILE_EVENT_REMOVED);
    RAD_ModelObservableClear(&tile->observable);
}

const char* RAD_ClientTileTypeText(RAD_ClientTileType_t type)
{
    switch(type)
    {
        case RAD_CLIENT_TILE_TYPE_GROUND:  return "ground";
        case RAD_CLIENT_TILE_TYPE_WATER:   return "water";
        case RAD_CLIENT_TILE_TYPE_VOID:    return "void";
        case RAD_CLIENT_TILE_TYPE_UNKNOWN: return "unbekannt";
    }
    return "unbekannt";
}

bool RAD_ClientTileHasUnit(const RAD_ClientTile_t *tile)
{
    return tile->unit != NULL;
}

bool RAD_ClientTileSubscribe(const RAD_ClientTile_t *tile, RAD_ClientTileObserver_t observer)
{
    const RAD_ModelObserver_t generic = {
        .user_argument = observer.user_argument,
        .callbacks = {
            [RAD_CLIENT_TILE_EVENT_CHANGED] = (RAD_ModelCallback_t)observer.changed,
            [RAD_CLIENT_TILE_EVENT_REMOVED] = (RAD_ModelCallback_t)observer.removed
        }
    };
    return RAD_ModelObservableSubscribe(RAD_ModelObservableOf(&tile->observable), &generic);
}

bool RAD_ClientTileUnsubscribe(const RAD_ClientTile_t *tile, const void *user_argument)
{
    return RAD_ModelObservableUnsubscribe(RAD_ModelObservableOf(&tile->observable), user_argument);
}
