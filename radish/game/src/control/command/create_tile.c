#include <radish/game/control/command/create_tile.h>


void RAD_SerializeCommandCreateTile(RAD_ByteWriter_t *writer, const RAD_CommandCreateTile_t *command)
{
    RAD_ByteWriteUint8(writer, RAD_TileTypeToWire(command->tile_type));
    RAD_ByteWriteInt16(writer, command->x);
    RAD_ByteWriteInt16(writer, command->y);
    RAD_ByteWriteInt8(writer, command->z);
}

RAD_CommandCodecResult_t RAD_DeserializeCommandCreateTile(RAD_ByteReader_t *reader, RAD_CommandCreateTile_t *command)
{
    uint8_t tile_type = 0;
    int16_t x = 0;
    int16_t y = 0;
    int8_t z = 0;

    if(!RAD_ByteReadUint8(reader, &tile_type) ||
       !RAD_ByteReadInt16(reader, &x) ||
       !RAD_ByteReadInt16(reader, &y) ||
       !RAD_ByteReadInt8(reader, &z))
    {
        return RAD_COMMAND_CODEC_ERROR_TRUNCATED;
    }

    bool ok = false;
    const RAD_TileType_t type = RAD_TileTypeFromWire(tile_type, &ok);
    if(!ok)
    {
        return RAD_COMMAND_CODEC_ERROR_UNKNOWN_TILE_TYPE;
    }

    command->tile_type = type;
    command->x = x;
    command->y = y;
    command->z = z;

    return RAD_COMMAND_CODEC_OK;
}
