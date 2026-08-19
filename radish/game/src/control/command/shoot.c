#include <radish/game/control/command/shoot.h>


void RAD_SerializeCommandShoot(RAD_ByteWriter_t *writer, const RAD_CommandShoot_t *command)
{
    RAD_ByteWriteInt32(writer, command->entity);
    RAD_ByteWriteInt16(writer, command->x);
    RAD_ByteWriteInt16(writer, command->y);
    RAD_ByteWriteUint8(writer, command->weapon);
}

RAD_CommandCodecResult_t RAD_DeserializeCommandShoot(RAD_ByteReader_t *reader, RAD_CommandShoot_t *command)
{
    RAD_EntityId_t entity = 0;
    int16_t x = 0;
    int16_t y = 0;
    uint8_t weapon = 0;

    if(!RAD_ByteReadInt32(reader, &entity) ||
       !RAD_ByteReadInt16(reader, &x) ||
       !RAD_ByteReadInt16(reader, &y) ||
       !RAD_ByteReadUint8(reader, &weapon))
    {
        return RAD_COMMAND_CODEC_ERROR_TRUNCATED;
    }

    command->entity = entity;
    command->x = x;
    command->y = y;

    // Ungeprueft uebernommen: siehe shoot.h.
    command->weapon = weapon;

    return RAD_COMMAND_CODEC_OK;
}
