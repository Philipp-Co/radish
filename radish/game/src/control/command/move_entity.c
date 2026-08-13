#include <radish/game/control/command/move_entity.h>


void RAD_SerializeCommandMoveEntity(RAD_ByteWriter_t *writer, const RAD_CommandMoveEntity_t *command)
{
    RAD_ByteWriteInt32(writer, command->entity);
    RAD_ByteWriteInt16(writer, command->from_x);
    RAD_ByteWriteInt16(writer, command->from_y);
    RAD_ByteWriteInt16(writer, command->to_x);
    RAD_ByteWriteInt16(writer, command->to_y);
}

RAD_CommandCodecResult_t RAD_DeserializeCommandMoveEntity(RAD_ByteReader_t *reader, RAD_CommandMoveEntity_t *command)
{
    RAD_EntityId_t entity = 0;
    int16_t from_x = 0;
    int16_t from_y = 0;
    int16_t to_x = 0;
    int16_t to_y = 0;

    if(!RAD_ByteReadInt32(reader, &entity) ||
       !RAD_ByteReadInt16(reader, &from_x) ||
       !RAD_ByteReadInt16(reader, &from_y) ||
       !RAD_ByteReadInt16(reader, &to_x) ||
       !RAD_ByteReadInt16(reader, &to_y))
    {
        return RAD_COMMAND_CODEC_ERROR_TRUNCATED;
    }

    command->entity = entity;
    command->from_x = from_x;
    command->from_y = from_y;
    command->to_x = to_x;
    command->to_y = to_y;

    return RAD_COMMAND_CODEC_OK;
}
