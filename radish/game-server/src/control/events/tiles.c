#include <radish/server/control/events/tiles.h>
#include <stdio.h>


void RAD_OnTileAddedToGame(void *user_argument, const RAD_Tile_t *tile)
{
    printf("Tile created Event! %i, %i\n", tile->x, tile->y);
}

void RAD_OnTileRemovedFromGame(void *user_argument, const RAD_Tile_t *tile)
{
    printf("Tile removed Event!\n");
}

void RAD_OnTileStateChanged(void *user_argument, const RAD_Tile_t *tile)
{
    printf("Tile chaged Event!\n");
}
