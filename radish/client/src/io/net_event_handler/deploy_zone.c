#include "common.h"

#include <stdio.h>

void RAD_IoNetOnDeployZone(void *user_argument, const RAD_NetDeployZone_t *zone)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    // Die Spieler-Ids sind auf beiden Seiten dieselben Zahlen (current_player.c).
    const RAD_ClientDeployZone_t client_zone = {
        .owner = (RAD_ClientPlayerId_t)zone->user,
        .min_x = zone->min.x,
        .min_y = zone->min.y,
        .max_x = zone->max.x,
        .max_y = zone->max.y
    };
    const bool taken = RAD_ClientWorldSetDeployZone(context->world, &client_zone);

    RAD_IoNetPrintDiscoverAnswerPrefix(context);
    printf("Aufstellzone 0x%llx = (%d, %d) bis (%d, %d)%s\n",
           (unsigned long long)zone->user,
           (int)zone->min.x, (int)zone->min.y, (int)zone->max.x, (int)zone->max.y,
           taken ? "" : " -- nicht uebernommen");
}
