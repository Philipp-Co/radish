#include <radish/game/events/event_manager.h>
#include <stddef.h>


static void RAD_DefaultOnTileAddedToGame(void *user_argument, const RAD_Tile_t *tile);
static void RAD_DefaultOnTileRemovedFromGame(void *user_argument, const RAD_Tile_t *tile);
static void RAD_DefaultOnTileStateChanged(void *user_argument, const RAD_Tile_t *tile);

static void RAD_DefaultOnMouseMove(void *user_argument, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y);
static void RAD_DefaultOnMousePressed(void *user_argument, int32_t x, int32_t y);
static void RAD_DefaultOnMouseReleased(void *user_argument, int32_t x, int32_t y);

RAD_EventManager_t RAD_CreateEventManager(void)
{
    RAD_EventManager_t manager = {
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
        }
    };
    return manager;
}

void RAD_DestroyEventManager(RAD_EventManager_t *manager)
{
    (void)manager;
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
    (void)tile;
}

static void RAD_DefaultOnTileRemovedFromGame(void *user_argument, const RAD_Tile_t *tile)
{
    (void)tile;
}

static void RAD_DefaultOnTileStateChanged(void *user_argument, const RAD_Tile_t *tile)
{
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
    (void)new_x;
    (void)new_y;
    (void)old_x;
    (void)old_y;
}

static void RAD_DefaultOnMousePressed(void *user_argument, int32_t x, int32_t y)
{
    (void)x;
    (void)y;
}

static void RAD_DefaultOnMouseReleased(void *user_argument, int32_t x, int32_t y)
{
    (void)x;
    (void)y;
}

