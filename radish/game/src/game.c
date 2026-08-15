#include <radish/game/game.h>
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

    game->world = RAD_CreateWorld(event_manager);
    RAD_InitWorld(&game->world);

    return game;
}

void RAD_DestroyGame(RAD_Game_t **game)
{
    free(*game);
    *game = NULL;
}

static void RAD_GameHandleMoveCommand(RAD_Game_t *game, RAD_Command_t *command)
{
    int32_t from_x = command->command.move_entity.from_x;
    int32_t from_y = command->command.move_entity.from_y;
    const RAD_Entity_t *source = RAD_WorldEntityAt(&game->world, command->command.move_entity.from_x, command->command.move_entity.from_y);
    int32_t to_x = command->command.move_entity.to_x;
    int32_t to_y = command->command.move_entity.to_y;
    const RAD_Entity_t *target = RAD_WorldEntityAt(&game->world, command->command.move_entity.to_x, command->command.move_entity.to_y);
    if(target != NULL)
    {
        printf("Unable to move! Tile already occupied...\n");
    }
    
    game->world.tiles[from_y][from_x].entity = RAD_ENTITY_NONE;
    game->world.tiles[to_y][to_x].entity = source->id;
    RAD_EventManagerPublishEntityMoved(game->event_manager, source, from_x, from_y, to_x, to_y);
}

void RAD_GameExecuteCommand(RAD_Game_t *game, RAD_Command_t *command)
{
    switch(command->header.type)
    {
        case RAD_COMMAND_TYPE_MOVE_ENTITY:
            RAD_GameHandleMoveCommand(game, command);
            break;
        default:
            break;
    }
}
