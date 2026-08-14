#ifndef __RAD_EVENTS_CALLBACKS_H__
#define __RAD_EVENTS_CALLBACKS_H__

#include <radish/game/tile.h>


typedef void (*RAD_OnMouseMove_t)(void *user_argument, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y);
typedef void (*RAD_OnMousePressed_t)(void *user_argument, int32_t x, int32_t y);
typedef void (*RAD_OnMouseReleased_t)(void *user_argument, int32_t x, int32_t y);

typedef struct 
{
    void *user_argument;
    RAD_OnMouseMove_t move;
    RAD_OnMousePressed_t pressed;
    RAD_OnMouseReleased_t released;
} RAD_EventsMouseCallbacks_t;


typedef void (*RAD_OnEntitySpawned_t)(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y);
typedef void (*RAD_OnEntityDestroyed_t)(void *user_argument, const RAD_Entity_t *entity, int32_t x, int32_t y);
typedef void (*RAD_OnEntityMoved_t)(void *user_argument, const RAD_Entity_t *entity, int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y);

typedef struct
{
    void *user_argument;
    RAD_OnEntitySpawned_t spawned;
    RAD_OnEntityDestroyed_t destroyed;
    RAD_OnEntityMoved_t moved;
} RAD_EventsEntityChangedCallback_t;

typedef void (*RAD_OnTileAddedToGame_t)(void *user_argument, const RAD_Tile_t *tile);
typedef void (*RAD_OnTileRemovedFromGame_t)(void *user_argument, const RAD_Tile_t *tile);
typedef void (*RAD_OnTileStateChanged_t)(void *user_argument, const RAD_Tile_t *tile);

typedef struct 
{
    void *user_argument;
    RAD_OnTileAddedToGame_t added;
    RAD_OnTileRemovedFromGame_t removed;
    RAD_OnTileStateChanged_t changed;
} RAD_EventsTileChangedCallback_t;

typedef struct {
    RAD_EventsEntityChangedCallback_t entity_changed_events;
    RAD_EventsTileChangedCallback_t tile_changed_events;
    RAD_EventsMouseCallbacks_t mouse_events;
} RAD_EventManager_t;


RAD_EventManager_t RAD_CreateEventManager(void);
void RAD_DestroyEventManager(RAD_EventManager_t *manager);

void RAD_EventManagerSubscribeToTileEvents(RAD_EventManager_t *manager, RAD_EventsTileChangedCallback_t callbacks);
void RAD_EventManagerPublishTileAddedToGameEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile);
void RAD_EventManagerPublishTileRemovedFromGameEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile);
void RAD_EventManagerPublishTileStateChangeEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile);

void RAD_EventManagerSubscribeToMouseEvents(RAD_EventManager_t *manager, RAD_EventsMouseCallbacks_t callbacks);
void RAD_EventManagerPublishMouseMoved(RAD_EventManager_t *manager, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y);
void RAD_EventManagerPublishMousePressed(RAD_EventManager_t *manager, int32_t x, int32_t y);
void RAD_EventManagerPublishMouseReleased(RAD_EventManager_t *manager, int32_t x, int32_t y);

void RAD_EventManagerSubscribeToEntityEvents(RAD_EventManager_t *manager, RAD_EventsEntityChangedCallback_t callbacks);
void RAD_EventManagerPublishEntitySpawned(RAD_EventManager_t *manager, const RAD_Entity_t *entity, int32_t x, int32_t y);
void RAD_EventManagerPublishEntityDestroyed(RAD_EventManager_t *manager, const RAD_Entity_t *entity, int32_t x, int32_t y);
void RAD_EventManagerPublishEntityMoved(RAD_EventManager_t *manager, const RAD_Entity_t *entity, int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y);

#endif
