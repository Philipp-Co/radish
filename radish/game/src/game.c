#include "radish/game/control/command/command.h"
#include "radish/game/control/events/event_manager.h"
#include "radish/game/model/world/world.h"
#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <radish/game/control/commands.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdio.h>


RAD_Game_t* RAD_CreateGame(RAD_EventManager_t *event_manager, RAD_UserId_t local_user)
{
    RAD_Game_t *game = malloc(sizeof(RAD_Game_t));
    if(game == NULL)
    {
        return NULL;
    }

    game->unit_pool = RAD_CreateUnitPool();
    if(game->unit_pool == NULL)
    {
        free(game);
        return NULL;
    }

    game->event_manager = event_manager;
    game->local_user = local_user;

    // Aus malloc kommt nichts Gesetztes: ohne diese beiden Zeilen traegt das
    // erste Kommando eine zufaellige Sequenznummer, und die Liste der
    // ausgefuehrten faengt an einem Zeiger an, den niemand geschrieben hat.
    game->current_sequence_number = 1;
    game->executed_commands.head = NULL;

    // Aus demselben Grund der Zug: ein nicht gesetzter Platz in seiner Reihe
    // waere ein Mitspieler mit erfundener Uuid. Niemand spielt mit, also ist auch
    // niemand dran -- der erste Beitritt eroeffnet den ersten Zug.
    game->turn = RAD_CreateTurn();
    game->started = false;
    game->finished = false;
    game->winner = RAD_USER_NONE;
    game->end_reason = RAD_GAME_END_NONE;
    game->number_of_players = 0;

    // Den Seed besorgt das Spiel selbst, und mit der eigenen Adresse als Salz:
    // gleichzeitig lebende Spiele liegen nie an derselben. Woraus der Seed sonst
    // besteht und warum, steht in control/dice.h.
    RAD_DiceSeed(&game->dice, RAD_DiceFreshSeed(game));

    RAD_CreateWorld(&game->world, event_manager, game->unit_pool);
    RAD_InitWorld(&game->world);

    return game;
}

void RAD_DestroyGame(RAD_Game_t **game)
{
    if(*game != NULL)
    {
        for(int32_t i = 0; i < (*game)->number_of_players; ++i)
        {
            RAD_DestroyPlayer(&(*game)->players[i]);
        }
        // Nach den Spielern: ihre Listen zeigen in den Pool.
        RAD_DestroyUnitPool(&(*game)->unit_pool);
    }
    free(*game);
    *game = NULL;
}

void RAD_GameExecuteCommand(RAD_Game_t *game, RAD_Command_t *command)
{
    // Vorbei ist vorbei: kein Kommando aendert mehr etwas (control/victory.c).
    if(game->finished)
    {
        return;
    }

    switch(command->header.type)
    {
        case RAD_COMMAND_TYPE_MOVE_UNIT:
            RAD_GameHandleMoveCommand(game, &command->command.move_unit);
            break;
        case RAD_COMMAND_TYPE_DEPLOY_UNIT:
            RAD_GameHandleDeployCommand(game, command->header.user, &command->command.deploy_unit);
            break;
        case RAD_COMMAND_TYPE_ATTACK:
            RAD_GameHandleAttackCommand(game, &command->command.attack);
            break;
        case RAD_COMMAND_TYPE_END_TURN:
            RAD_GameHandleEndTurnCommand(game, command->header.user);
            break;
        default:
            break;
    }
}
