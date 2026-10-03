#include "common.h"

#include <stdio.h>

void RAD_IoNetOnCurrentPlayer(void *user_argument, uint64_t user_id)
{
    const RAD_IoNetEventHandlerContext_t *context = (const RAD_IoNetEventHandlerContext_t*)user_argument;

    RAD_IoNetPrintDiscoverAnswerPrefix(context);
    if(user_id == 0)
    {
        printf("am Zug = niemand\n");
        return;
    }
    printf("am Zug = 0x%llx\n", (unsigned long long)user_id);
}
