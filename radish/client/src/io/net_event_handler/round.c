#include "common.h"

#include <stdio.h>

void RAD_IoNetOnRound(void *user_argument, uint32_t round, uint32_t rounds)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    // Mehr als in einen int32 passt, hat kein Spiel.
    RAD_ClientWorldSetRound(context->world,
                            (round > (uint32_t)INT32_MAX) ? INT32_MAX : (int32_t)round,
                            (rounds > (uint32_t)INT32_MAX) ? INT32_MAX : (int32_t)rounds);

    // Ohne den Discover-Vorspann, wie beim Zug: das Ereignis kommt auch nach jedem
    // Abgeben, ohne dass jemand gefragt hat.
    printf("<- Runde %u/%u\n", (unsigned)round, (unsigned)rounds);
}
