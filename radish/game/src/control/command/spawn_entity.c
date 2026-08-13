#include <radish/game/control/command/spawn_entity.h>


void RAD_SerializeCommandSpawnEntity(RAD_ByteWriter_t *writer, const RAD_CommandSpawnEntity_t *command)
{
    RAD_ByteWriteUint8(writer, RAD_EntityTypeToWire(command->entity_type));
    RAD_ByteWriteInt16(writer, command->x);
    RAD_ByteWriteInt16(writer, command->y);
    RAD_ByteWriteInt8(writer, command->z);
}

RAD_CommandCodecResult_t RAD_DeserializeCommandSpawnEntity(RAD_ByteReader_t *reader, RAD_CommandSpawnEntity_t *command)
{
    // Erst vollstaendig in eigene Variablen lesen und "command" zuletzt in einem
    // Zug setzen: so bleibt es bei jedem Fehler unberuehrt.
    uint8_t entity_type = 0;
    int16_t x = 0;
    int16_t y = 0;
    int8_t z = 0;

    if(!RAD_ByteReadUint8(reader, &entity_type) ||
       !RAD_ByteReadInt16(reader, &x) ||
       !RAD_ByteReadInt16(reader, &y) ||
       !RAD_ByteReadInt8(reader, &z))
    {
        return RAD_COMMAND_CODEC_ERROR_TRUNCATED;
    }

    bool ok = false;
    const RAD_EntityType_t type = RAD_EntityTypeFromWire(entity_type, &ok);
    if(!ok)
    {
        return RAD_COMMAND_CODEC_ERROR_UNKNOWN_ENTITY_TYPE;
    }

    command->entity_type = type;
    command->x = x;
    command->y = y;
    command->z = z;

    return RAD_COMMAND_CODEC_OK;
}
