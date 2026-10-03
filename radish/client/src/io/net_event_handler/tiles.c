#include "common.h"

#include <stdio.h>

void RAD_IoNetOnTiles(void *user_argument, const RAD_NetTile_t *tiles, size_t number_of_tiles)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    if(number_of_tiles == 0)
    {
        RAD_IoNetPrintDiscoverAnswerPrefix(context);
        printf("keine Felder\n");
        return;
    }

    for(size_t i = 0; i < number_of_tiles; ++i)
    {
        RAD_IoNetPrintDiscoverAnswerPrefix(context);
        RAD_IoNetPrintTile(&tiles[i]);
        printf("\n");
        RAD_IoNetApplyTile(context, &tiles[i]);
    }
}
