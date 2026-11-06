#include "common.h"

#include <stdio.h>

void RAD_IoNetOnPlayers(void *user_argument, const uint64_t *user_ids, size_t number_of_users)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    // 0 heisst niemand, auf beiden Seiten (RAD_NET_USER_NONE, RAD_CLIENT_PLAYER_NONE)
    // -- schon hier weglassen, damit er keinen Platz belegt.
    RAD_ClientPlayerId_t players[RAD_CLIENT_WORLD_PLAYERS];
    size_t count = 0;
    for(size_t i = 0; (i < number_of_users) && (count < RAD_CLIENT_WORLD_PLAYERS); ++i)
    {
        if(user_ids[i] != 0)
        {
            players[count++] = (RAD_ClientPlayerId_t)user_ids[i];
        }
    }
    RAD_ClientWorldSetPlayers(context->world, players, count);

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
