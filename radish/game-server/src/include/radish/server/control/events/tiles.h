#ifndef __RAD_SERVER_TILE_EVENTS_H__
#define __RAD_SERVER_TILE_EVENTS_H__

#include <radish/game/control/events/event_manager.h>

void RAD_OnTileAddedToGame(void *user_argument, const RAD_Tile_t *tile);
void RAD_OnTileRemovedFromGame(void *user_argument, const RAD_Tile_t *tile);
void RAD_OnTileStateChanged(void *user_argument, const RAD_Tile_t *tile);

#endif
