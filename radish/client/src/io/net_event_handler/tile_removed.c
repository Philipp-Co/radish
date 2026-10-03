#include <radish/io/net_event_handler.h>

#include <stdio.h>

void RAD_IoNetOnTileRemoved(void *user_argument, uint32_t x, uint32_t y)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    printf("<- Tile entfernt (%u, %u)\n", x, y);
    RAD_ClientWorldRemoveTile(context->world, x, y);
}
