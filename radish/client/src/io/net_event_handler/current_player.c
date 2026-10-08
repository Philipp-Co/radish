#include "common.h"

#include <stdio.h>

void RAD_IoNetOnCurrentPlayer(void *user_argument, uint64_t user_id)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    // 0 heisst niemand, auf beiden Seiten (RAD_NET_USER_NONE, RAD_CLIENT_PLAYER_NONE).
    RAD_ClientWorldSetCurrentPlayer(context->world, (RAD_ClientPlayerId_t)user_id);

    // Ohne den Discover-Vorspann (RAD_IoNetPrintDiscoverAnswerPrefix): das
    // Ereignis kommt auch nach jedem Abgeben, ohne dass jemand gefragt hat.
    if(user_id == 0)
    {
        printf("<- am Zug = niemand\n");
        return;
    }
    printf("<- am Zug = 0x%llx\n", (unsigned long long)user_id);
}
