#include <radish/game/control/command/use.h>


void RAD_SerializeCommandUse(RAD_ByteWriter_t *writer, const RAD_CommandUse_t *command)
{
    RAD_ByteWriteInt32(writer, command->entity);
    RAD_ByteWriteInt16(writer, command->x);
    RAD_ByteWriteInt16(writer, command->y);
}

RAD_CommandCodecResult_t RAD_DeserializeCommandUse(RAD_ByteReader_t *reader, RAD_CommandUse_t *command)
{
    RAD_EntityId_t entity = 0;
    int16_t x = 0;
    int16_t y = 0;

    if(!RAD_ByteReadInt32(reader, &entity) ||
       !RAD_ByteReadInt16(reader, &x) ||
       !RAD_ByteReadInt16(reader, &y))
    {
        return RAD_COMMAND_CODEC_ERROR_TRUNCATED;
    }

    command->entity = entity;
    command->x = x;
    command->y = y;

    return RAD_COMMAND_CODEC_OK;
}
