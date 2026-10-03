#include "common.h"

#include <stdio.h>

void RAD_IoNetOnPlayers(void *user_argument, const uint64_t *user_ids, size_t number_of_users)
{
    const RAD_IoNetEventHandlerContext_t *context = (const RAD_IoNetEventHandlerContext_t*)user_argument;

    RAD_IoNetPrintDiscoverAnswerPrefix(context);
    printf("Mitspieler (%zu) =", number_of_users);
    if(number_of_users == 0)
    {
        printf(" keine");
    }
    for(size_t i = 0; i < number_of_users; ++i)
    {
        printf(" 0x%llx", (unsigned long long)user_ids[i]);
    }
    printf("\n");
}
