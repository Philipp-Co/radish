#include "common.h"

#include <stdio.h>

void RAD_IoNetOnTileCreated(void *user_argument, const RAD_NetTile_t *tile)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    printf("<- Tile erstellt: ");
    RAD_IoNetPrintTile(tile);
    printf("\n");
    RAD_IoNetApplyTile(context, tile);
}
