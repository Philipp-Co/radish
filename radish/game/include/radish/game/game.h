#ifndef __RAD_GAME_H__
#define __RAD_GAME_H__

#include "radish/game/entity.h"
#include <radish/game/world.h>
#include <radish/game/events/event_manager.h>
#include <radish/game/control/command/command.h>

struct RAD_CommandListItem
{
    RAD_Command_t command;
    struct RAD_CommandListItem *next;
};

typedef struct
{
    struct RAD_CommandListItem *head;
} RAD_CommandList_t;

///
/// Oberste Abstraktion. Ein Spiel besteht aus genau einer Welt; sie liegt per
/// Wert im Spiel, damit die Ownership eindeutig ist und es nur eine Allokation
/// gibt.
///
typedef struct
{
    RAD_World_t world;
    RAD_EventManager_t *event_manager; 
    RAD_CommandList_t executed_commands;
    
    uint32_t current_sequence_number;
} RAD_Game_t;


RAD_Game_t* RAD_CreateGame(RAD_EventManager_t *event_manager);
void RAD_DestroyGame(RAD_Game_t **game);

bool RAD_GameSpawnEntity(RAD_Game_t *game, RAD_EntityType_t type, int32_t x, int32_t y, int32_t z, RAD_Command_t *output);
bool RAD_GameDestroyEntity(RAD_Game_t *game, RAD_EntityId_t id, RAD_Command_t *output);
bool RAD_GameMoveEntity(RAD_Game_t *game, RAD_EntityId_t id, int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y, RAD_Command_t *output);

void RAD_GameExecuteCommand(RAD_Game_t *game, RAD_Command_t *command);
void RAD_GameRollbackLastCommand(RAD_Game_t *game);

#endif
