#include "common.h"

#include <stdio.h>

void RAD_IoNetOnGameFinished(void *user_argument, const RAD_NetGameFinished_t *finished)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    // Die Spieler-Ids sind auf beiden Seiten dieselben Zahlen (current_player.c).
    RAD_ClientGameResult_t result = {
        .finished = true,
        .winner = (RAD_ClientPlayerId_t)finished->winner,
        .eliminated = (finished->reason == RAD_NET_GAME_END_ELIMINATED),
        .number_of_players = 0
    };
    for(size_t i = 0; (i < finished->number_of_players) && (result.number_of_players < RAD_CLIENT_WORLD_PLAYERS); ++i)
    {
        result.players[result.number_of_players] = (RAD_ClientPlayerId_t)finished->users[i];
        result.scores[result.number_of_players] = (finished->scores[i] > (uint32_t)INT32_MAX) ? INT32_MAX : (int32_t)finished->scores[i];
        result.number_of_players++;
    }
    RAD_ClientWorldSetResult(context->world, &result);

    if(finished->winner == RAD_NET_USER_NONE)
    {
        printf("<- Spiel beendet: unentschieden");
    }
    else
    {
        printf("<- Spiel beendet: Sieger 0x%llx", (unsigned long long)finished->winner);
    }
    printf(" (%s)", result.eliminated ? "keine Einheiten mehr" : "nach Punkten");
    for(int32_t i = 0; i < result.number_of_players; ++i)
    {
        printf(", 0x%llx: %d", (unsigned long long)result.players[i], (int)result.scores[i]);
    }
    printf("\n");
}
