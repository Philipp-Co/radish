#include <radish/game/game.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>


RAD_Game_t* RAD_CreateGame(RAD_EventManager_t *event_manager)
{
    RAD_Game_t *game = malloc(sizeof(RAD_Game_t));
    if(game == NULL)
    {
        return NULL;
    }

    game->event_manager = event_manager;
    game->world = RAD_CreateWorld(event_manager);
    RAD_InitWorld(&game->world);

    return game;
}

void RAD_DestroyGame(RAD_Game_t **game)
{
    free(*game);
    *game = NULL;
}

