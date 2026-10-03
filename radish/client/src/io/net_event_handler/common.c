#include "common.h"

#include <stdio.h>

void RAD_IoNetPrintDiscoverAnswerPrefix(const RAD_IoNetEventHandlerContext_t *context)
{
    const RAD_IoNetDiscoverRequest_t *request = &context->session->last_discover_request;

    if(!request->sent)
    {
        printf("<- Discover-Antwort (ohne eigene Anfrage): ");
        return;
    }

    printf("<- Discover-Antwort auf x=%u y=%u w=%u h=%u: ",
        (unsigned)request->x, (unsigned)request->y,
        (unsigned)request->w, (unsigned)request->h);
}

void RAD_IoNetPrintTile(const RAD_NetTile_t *tile)
{
    printf("Feld (%u, %u) Typ=%s z=%u Figur=",
        (unsigned)tile->x, (unsigned)tile->y, RAD_NetTileTypeText(tile->type), (unsigned)tile->z);

    // -1 ist RAD_NET_ENTITY_NONE; jede andere Zahl ist eine Figur, auch die 0.
    if(tile->entity_id == RAD_NET_ENTITY_NONE)
    {
        printf("keine");
    }
    else
    {
        printf("%d", (int)tile->entity_id);
    }
}

void RAD_IoNetApplyTile(RAD_IoNetEventHandlerContext_t *context, const RAD_NetTile_t *tile)
{
    RAD_ClientWorldApplyTile(context->world, tile);
}
