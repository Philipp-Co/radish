#include <radish/game/control/events/event_manager.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>


///
/// Die Struktur steht hier und nicht im Header -- dieselbe Ueberlegung wie bei
/// struct RAD_Control im Server: niemand sonst braucht sie, und was niemand sieht,
/// kann auch niemand an den Subscribe-Funktionen vorbei setzen.
///
struct RAD_EventManager
{
    RAD_EventsUnitChangedCallback_t unit_changed_events;
    RAD_EventsTileChangedCallback_t tile_changed_events;
    RAD_EventsMouseCallbacks_t mouse_events;
    RAD_EventsTurnCallback_t turn_events;
};


static void RAD_DefaultOnTileAddedToGame(void *user_argument, const RAD_Tile_t *tile);
static void RAD_DefaultOnTileRemovedFromGame(void *user_argument, const RAD_Tile_t *tile);
static void RAD_DefaultOnTileStateChanged(void *user_argument, const RAD_Tile_t *tile);

static void RAD_DefaultOnMouseMove(void *user_argument, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y);
static void RAD_DefaultOnMousePressed(void *user_argument, int32_t x, int32_t y);
static void RAD_DefaultOnMouseReleased(void *user_argument, int32_t x, int32_t y);

static void RAD_DefaultUnit(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y);
static void RAD_DefaultUnitMove(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result);

static void RAD_DefaultOnTurnChanged(void *user_argument, RAD_UserId_t current_user);


RAD_EventManager_t* RAD_CreateEventManager(void)
{
    RAD_EventManager_t *manager = malloc(sizeof(struct RAD_EventManager));
    if(manager == NULL)
    {
        return NULL;
    }

    *manager = (struct RAD_EventManager){
        .tile_changed_events = {
            .user_argument = NULL,
            .added=RAD_DefaultOnTileAddedToGame,
            .removed=RAD_DefaultOnTileRemovedFromGame,
            .changed=RAD_DefaultOnTileStateChanged
        },
        .mouse_events = {
            .user_argument = NULL,
            .move=RAD_DefaultOnMouseMove,
            .pressed=RAD_DefaultOnMousePressed,
            .released=RAD_DefaultOnMouseReleased
        },
        .unit_changed_events = {
            .user_argument = NULL,
            .spawned = RAD_DefaultUnit,
            .destroyed = RAD_DefaultUnit,
            .moved = RAD_DefaultUnitMove
        },
        .turn_events = {
            .user_argument = NULL,
            .changed = RAD_DefaultOnTurnChanged
        }
    };

    return manager;
}

void RAD_DestroyEventManager(RAD_EventManager_t **manager)
{
    free(*manager);
    *manager = NULL;
}

void RAD_EventManagerSubscribeToTileEvents(RAD_EventManager_t *manager, RAD_EventsTileChangedCallback_t callbacks)
{
    manager->tile_changed_events = callbacks;
}

void RAD_EventManagerPublishTileAddedToGameEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile)
{
    manager->tile_changed_events.added(manager->tile_changed_events.user_argument, tile);
}

void RAD_EventManagerPublishTileRemovedFromGameEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile)
{
    manager->tile_changed_events.removed(manager->tile_changed_events.user_argument, tile);
}

void RAD_EventManagerPublishTileStateChangeEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile)
{
    manager->tile_changed_events.changed(manager->tile_changed_events.user_argument, tile);
}


static void RAD_DefaultOnTileAddedToGame(void *user_argument, const RAD_Tile_t *tile)
{
    (void)user_argument;
    (void)tile;
}

static void RAD_DefaultOnTileRemovedFromGame(void *user_argument, const RAD_Tile_t *tile)
{
    (void)user_argument;
    (void)tile;
}

static void RAD_DefaultOnTileStateChanged(void *user_argument, const RAD_Tile_t *tile)
{
    (void)user_argument;
    (void)tile;
}


void RAD_EventManagerSubscribeToMouseEvents(RAD_EventManager_t *manager, RAD_EventsMouseCallbacks_t callbacks)
{
    manager->mouse_events = callbacks;
}

void RAD_EventManagerPublishMouseMoved(RAD_EventManager_t *manager, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y)
{
    manager->mouse_events.move(manager->mouse_events.user_argument, new_x, new_y, old_x, old_y);
}

void RAD_EventManagerPublishMousePressed(RAD_EventManager_t *manager, int32_t x, int32_t y)
{
    manager->mouse_events.pressed(manager->mouse_events.user_argument, x, y);
}

void RAD_EventManagerPublishMouseReleased(RAD_EventManager_t *manager, int32_t x, int32_t y)
{
    manager->mouse_events.released(manager->mouse_events.user_argument, x, y);
}

static void RAD_DefaultOnMouseMove(void *user_argument, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y)
{
    (void)user_argument;
    (void)new_x;
    (void)new_y;
    (void)old_x;
    (void)old_y;
}

static void RAD_DefaultOnMousePressed(void *user_argument, int32_t x, int32_t y)
{
    (void)user_argument;
    (void)x;
    (void)y;
}

static void RAD_DefaultOnMouseReleased(void *user_argument, int32_t x, int32_t y)
{
    (void)user_argument;
    (void)x;
    (void)y;
}

void RAD_EventManagerSubscribeToUnitEvents(RAD_EventManager_t *manager, RAD_EventsUnitChangedCallback_t callbacks)
{
    manager->unit_changed_events = callbacks;
}

void RAD_EventManagerPublishUnitSpawned(RAD_EventManager_t *manager, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    manager->unit_changed_events.spawned(manager->unit_changed_events.user_argument, unit, x, y);
}

void RAD_EventManagerPublishUnitDestroyed(RAD_EventManager_t *manager, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    manager->unit_changed_events.destroyed(manager->unit_changed_events.user_argument, unit, x, y);
}

void RAD_EventManagerPublishUnitMoved(RAD_EventManager_t *manager, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    manager->unit_changed_events.moved(manager->unit_changed_events.user_argument, unit, path, result);
}

static void RAD_DefaultUnit(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)user_argument;
    (void)unit;
    (void)x;
    (void)y;
}

static void RAD_DefaultUnitMove(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    (void)user_argument;
    (void)unit;
    (void)path;
    (void)result;
}

void RAD_EventManagerSubscribeToTurnEvents(RAD_EventManager_t *manager, RAD_EventsTurnCallback_t callbacks)
{
    manager->turn_events = callbacks;
}

void RAD_EventManagerPublishTurnChanged(RAD_EventManager_t *manager, RAD_UserId_t current_user)
{
    manager->turn_events.changed(manager->turn_events.user_argument, current_user);
}

static void RAD_DefaultOnTurnChanged(void *user_argument, RAD_UserId_t current_user)
{
    (void)user_argument;
    (void)current_user;
}
