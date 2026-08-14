#include "radish/game/control/command/command.h"
#include <radish/game/game.h>


bool RAD_GameSpawnEntity(RAD_Game_t *game, RAD_EntityType_t type, int32_t x, int32_t y, int32_t z, RAD_Command_t *output)
{
    output->header.type = RAD_COMMAND_TYPE_SPAWN_ENTITY;
    output->header.sequence = game->current_sequence_number++; 

    output->command.spawn_entity.entity_type = type; 
    output->command.spawn_entity.x = x;
    output->command.spawn_entity.y = y;
    output->command.spawn_entity.z = z;
    
    return true;
}

bool RAD_GameDestroyEntity(RAD_Game_t *game, RAD_EntityId_t id, RAD_Command_t *output)
{
    output->header.type = RAD_COMMAND_TYPE_REMOVE_ENTITY;
    output->header.sequence = game->current_sequence_number++; 

    output->command.remove_entity.entity = id;
    return true;
}

bool RAD_GameMoveEntity(RAD_Game_t *game, RAD_EntityId_t id, int32_t from_x, int32_t from_y, int32_t to_x, int32_t to_y, RAD_Command_t *output)
{
    output->header.type = RAD_COMMAND_TYPE_MOVE_ENTITY;
    output->header.sequence = game->current_sequence_number++;

    output->command.move_entity.entity = id; 
    output->command.move_entity.from_x = from_x;
    output->command.move_entity.from_y = from_y;
    output->command.move_entity.to_x = to_x;
    output->command.move_entity.to_y = to_y;

    return true;
}
