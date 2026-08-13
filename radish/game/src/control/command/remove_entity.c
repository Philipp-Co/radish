#include <radish/game/control/command/remove_entity.h>


void RAD_SerializeCommandRemoveEntity(RAD_ByteWriter_t *writer, const RAD_CommandRemoveEntity_t *command)
{
    RAD_ByteWriteInt32(writer, command->entity);
}

RAD_CommandCodecResult_t RAD_DeserializeCommandRemoveEntity(RAD_ByteReader_t *reader, RAD_CommandRemoveEntity_t *command)
{
    RAD_EntityId_t entity = 0;

    if(!RAD_ByteReadInt32(reader, &entity))
    {
        return RAD_COMMAND_CODEC_ERROR_TRUNCATED;
    }

    command->entity = entity;

    return RAD_COMMAND_CODEC_OK;
}
