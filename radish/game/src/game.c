#include "radish/game/control/command/command.h"
#include "radish/game/control/events/event_manager.h"
#include "radish/game/model/world/world.h"
#include <radish/game/game.h>
#include <radish/game/model/game.h>
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

static void RAD_GameHandleMoveCommand(RAD_Game_t *game, RAD_CommandMoveEntity_t *move)
{
    int32_t result = -1;
    //
    // Der valide Pfad ist zum Beginn leer. Er fuellt sich mit jedem
    // Schleifendurchlauf um ein weiteres Feld -- und faengt mit dem an, auf dem die
    // Figur schon steht, damit er dieselbe Gestalt hat wie der hereingekommene
    // (path.h): erstes Feld der Standort, letztes das Ziel.
    //
    RAD_EntityPath_t valid_path = {
        .number_of_steps = 0
    };
    RAD_Entity_t *entity = RAD_WorldEntityById(&game->world, move->entity);
    if(NULL == entity)
    {
        printf("Entity with Id %i does not exist!\n", move->entity);
        goto end;
    }
    //
    // Erstmal pruefen.
    //
    int32_t x = entity->x;
    int32_t y = entity->y;

    //
    // Der Standort ist das erste Feld des gelaufenen Weges. Er wird nicht geprueft:
    // dort steht die Figur, also ist das Feld besetzt -- von ihr selbst.
    //
    valid_path.steps_to[0].x = (int16_t)x;
    valid_path.steps_to[0].y = (int16_t)y;
    valid_path.number_of_steps = 1;

    //
    // Ab eins und nicht ab null: steps_to[0] ist der Standort und kein Schritt
    // (path.h). Bei null zu beginnen hiesse, das Feld unter der Figur auf Belegung
    // zu pruefen -- es ist belegt, und jede Bewegung endete sofort.
    //
    // Was in steps_to[0] steht, wird dabei nicht mit dem Standort verglichen. Ein
    // zweimal zugestelltes Kommando muss zum selben Ergebnis fuehren (command.h);
    // beim zweiten Mal steht die Figur schon am Ziel, und ein Vergleich waere dann
    // ein Fehlschlag, wo keiner sein soll.
    //
    uint32_t i = 1;
    for(;i<((uint32_t)move->path.number_of_steps); ++i)
    {
        //
        // Check if the next Tile is empty.
        //
        const int16_t tmp_x = move->path.steps_to[i].x;
        const int16_t tmp_y = move->path.steps_to[i].y;
        const RAD_Tile_t *tile = RAD_WorldTileAt(&game->world, tmp_x, tmp_y);
        if(NULL == tile)
        {
            valid_path.number_of_steps = 0;
            goto end;
        }
        else if(RAD_ENTITY_NONE != tile->entity)
        {
            //
            // Sobald die naechste Tile besetzt ist, hoert die Bewegung auf.
            //
            result = 0;
            break;
        }
        x = tmp_x;
        y = tmp_y;
        valid_path.steps_to[valid_path.number_of_steps].x = x;
        valid_path.steps_to[valid_path.number_of_steps].y = y;
        valid_path.number_of_steps++;
    }
    result = 0;

    //
    // Nur der Standort im Pfad heisst: kein Schritt ging durch. Dann bleibt der
    // Pfad leer -- ein Weg aus einem Feld ist keiner, und ein Abonnent soll nicht
    // erst nachrechnen muessen, ob sich etwas bewegt hat.
    //
    if(valid_path.number_of_steps < 2)
    {
        valid_path.number_of_steps = 0;
    }

    //
    // Jetzt erst schreiben.
    // 
    game->world.tiles[entity->y][entity->x].entity = RAD_ENTITY_NONE;
    game->world.tiles[y][x].entity = entity->id;
    entity->x = x;
    entity->y = y;

end:
    //
    // Beobachter benachrichtigen...
    //
    RAD_EventManagerPublishEntityMoved(game->event_manager, entity, &valid_path, result);
}


static void RAD_GameHandleSpawnCommand(RAD_Game_t *game, RAD_CommandSpawnEntity_t *spawn)
{
    const int16_t x = spawn->x;
    const int16_t y = spawn->y;
    const RAD_Entity_t *entity = RAD_WorldEntityAt(&game->world, x, y);
    if(NULL != entity)
    {
        printf("Unable to spawn Entity at (%i, %i)\n", x, y);
        return;
    }
    const RAD_EntityId_t id = RAD_WorldSpawnEntity(&game->world, spawn->entity_type, x, y); 
    const RAD_Entity_t *new_entity = RAD_WorldEntityById(&game->world, id);

    RAD_EventManagerPublishEntitySpawned(game->event_manager, new_entity, x, y);
}

void RAD_GameExecuteCommand(RAD_Game_t *game, RAD_Command_t *command)
{
    switch(command->header.type)
    {
        case RAD_COMMAND_TYPE_MOVE_ENTITY:
            RAD_GameHandleMoveCommand(game, &command->command.move_entity);
            break;
        case RAD_COMMAND_TYPE_SPAWN_ENTITY:
            RAD_GameHandleSpawnCommand(game, &command->command.spawn_entity);
        default:
            break;
    }
}
