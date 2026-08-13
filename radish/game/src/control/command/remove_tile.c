#include <radish/game/control/command/remove_tile.h>


void RAD_SerializeCommandRemoveTile(RAD_ByteWriter_t *writer, const RAD_CommandRemoveTile_t *command)
{
    RAD_ByteWriteInt16(writer, command->x);
    RAD_ByteWriteInt16(writer, command->y);
}

RAD_CommandCodecResult_t RAD_DeserializeCommandRemoveTile(RAD_ByteReader_t *reader, RAD_CommandRemoveTile_t *command)
{
    int16_t x = 0;
    int16_t y = 0;

    if(!RAD_ByteReadInt16(reader, &x) ||
       !RAD_ByteReadInt16(reader, &y))
    {
        return RAD_COMMAND_CODEC_ERROR_TRUNCATED;
    }

    command->x = x;
    command->y = y;

    return RAD_COMMAND_CODEC_OK;
}
