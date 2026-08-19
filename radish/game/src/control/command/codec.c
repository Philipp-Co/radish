#include <radish/game/control/command/codec.h>
#include <radish/game/control/command/spawn_entity.h>
#include <radish/game/control/command/move_entity.h>
#include <radish/game/control/command/remove_entity.h>
#include <radish/game/control/command/create_tile.h>
#include <radish/game/control/command/remove_tile.h>
#include <radish/game/control/command/shoot.h>
#include <radish/game/control/command/use.h>

///
/// Die Wire-Nummern stehen nur hier, in je zwei Richtungen ausgeschrieben. Keine
/// Tabelle, die sich indizieren liesse: genau das waere die Kopplung an die
/// Reihenfolge im enum, die das Format nicht haben soll. Wer eine Art hinzufuegt,
/// ergaenzt beide Richtungen und bekommt fuer die Serialisierung vom Compiler
/// gesagt, dass ein Zweig fehlt (-Wswitch).
///

const char* RAD_CommandCodecResultText(RAD_CommandCodecResult_t result)
{
    switch(result)
    {
        case RAD_COMMAND_CODEC_OK:                           return "in Ordnung";
        case RAD_COMMAND_CODEC_ERROR_TRUNCATED:              return "Nachricht endet mitten im Kommando";
        case RAD_COMMAND_CODEC_ERROR_TRAILING_BYTES:         return "Bytes hinter dem Kommando";
        case RAD_COMMAND_CODEC_ERROR_UNKNOWN_COMMAND_TYPE:   return "unbekannte Kommandoart";
        case RAD_COMMAND_CODEC_ERROR_UNKNOWN_ENTITY_TYPE:    return "unbekannter Entitaetstyp";
        case RAD_COMMAND_CODEC_ERROR_UNKNOWN_TILE_TYPE:      return "unbekannter Tile-Typ";
        case RAD_COMMAND_CODEC_ERROR_HEADER_MISMATCH:        return "Koepfe der Antwort tragen nicht dasselbe";
        case RAD_COMMAND_CODEC_ERROR_INVALID_STEP_COUNT:     return "unmoegliche Anzahl von Schritten";
        default:                                             return "unbekanntes Ergebnis";
    }
}

uint8_t RAD_CommandTypeToWire(RAD_CommandType_t type)
{
    switch(type)
    {
        case RAD_COMMAND_TYPE_SPAWN_ENTITY:  return 1;
        case RAD_COMMAND_TYPE_MOVE_ENTITY:   return 2;
        case RAD_COMMAND_TYPE_REMOVE_ENTITY: return 3;
        case RAD_COMMAND_TYPE_CREATE_TILE:   return 4;
        case RAD_COMMAND_TYPE_REMOVE_TILE:   return 5;
        case RAD_COMMAND_TYPE_END_TURN:      return 6;
        case RAD_COMMAND_TYPE_SHOOT:         return 7;
        case RAD_COMMAND_TYPE_USE:           return 8;
        case RAD_COMMAND_TYPE_NONE:
        default:                             return 0;
    }
}

RAD_CommandType_t RAD_CommandTypeFromWire(uint8_t wire, bool *ok)
{
    RAD_CommandType_t type = RAD_COMMAND_TYPE_NONE;

    switch(wire)
    {
        case 1: type = RAD_COMMAND_TYPE_SPAWN_ENTITY;  break;
        case 2: type = RAD_COMMAND_TYPE_MOVE_ENTITY;   break;
        case 3: type = RAD_COMMAND_TYPE_REMOVE_ENTITY; break;
        case 4: type = RAD_COMMAND_TYPE_CREATE_TILE;   break;
        case 5: type = RAD_COMMAND_TYPE_REMOVE_TILE;   break;
        case 6: type = RAD_COMMAND_TYPE_END_TURN;      break;
        case 7: type = RAD_COMMAND_TYPE_SHOOT;         break;
        case 8: type = RAD_COMMAND_TYPE_USE;           break;
        default:
            if(ok != NULL)
            {
                *ok = false;
            }
            return RAD_COMMAND_TYPE_NONE;
    }

    if(ok != NULL)
    {
        *ok = true;
    }
    return type;
}

uint8_t RAD_EntityTypeToWire(RAD_EntityType_t type)
{
    switch(type)
    {
        case RAD_ENTITY_TYPE_PLAYER: return 1;
        case RAD_ENTITY_TYPE_NPC:    return 2;
        case RAD_ENTITY_TYPE_NONE:
        default:                     return 0;
    }
}

RAD_EntityType_t RAD_EntityTypeFromWire(uint8_t wire, bool *ok)
{
    RAD_EntityType_t type = RAD_ENTITY_TYPE_NONE;

    switch(wire)
    {
        case 1: type = RAD_ENTITY_TYPE_PLAYER; break;
        case 2: type = RAD_ENTITY_TYPE_NPC;    break;
        default:
            if(ok != NULL)
            {
                *ok = false;
            }
            return RAD_ENTITY_TYPE_NONE;
    }

    if(ok != NULL)
    {
        *ok = true;
    }
    return type;
}

uint8_t RAD_TileTypeToWire(RAD_TileType_t type)
{
    switch(type)
    {
        case RAD_TILE_TYPE_VOID:   return 1;
        case RAD_TILE_TYPE_GROUND: return 2;
        case RAD_TILE_TYPE_WATER:  return 3;
        default:                   return 0;
    }
}

RAD_TileType_t RAD_TileTypeFromWire(uint8_t wire, bool *ok)
{
    RAD_TileType_t type = RAD_TILE_TYPE_VOID;

    switch(wire)
    {
        case 1: type = RAD_TILE_TYPE_VOID;   break;
        case 2: type = RAD_TILE_TYPE_GROUND; break;
        case 3: type = RAD_TILE_TYPE_WATER;  break;
        default:
            if(ok != NULL)
            {
                *ok = false;
            }
            return RAD_TILE_TYPE_VOID;
    }

    if(ok != NULL)
    {
        *ok = true;
    }
    return type;
}

void RAD_SerializeCommandHeader(RAD_ByteWriter_t *writer, const RAD_CommandHeader_t *header)
{
    RAD_ByteWriteUint8(writer, RAD_CommandTypeToWire(header->type));
    RAD_ByteWriteUint64(writer, header->sequence);
    RAD_ByteWriteUint64(writer, header->user);
}

RAD_CommandCodecResult_t RAD_DeserializeCommandHeader(RAD_ByteReader_t *reader, RAD_CommandHeader_t *header)
{
    uint8_t wire_type = 0;
    RAD_CommandSequence_t sequence = 0;
    RAD_UserId_t user = RAD_USER_NONE;

    if(!RAD_ByteReadUint8(reader, &wire_type) ||
       !RAD_ByteReadUint64(reader, &sequence) ||
       !RAD_ByteReadUint64(reader, &user))
    {
        return RAD_COMMAND_CODEC_ERROR_TRUNCATED;
    }

    bool ok = false;
    const RAD_CommandType_t type = RAD_CommandTypeFromWire(wire_type, &ok);
    if(!ok)
    {
        return RAD_COMMAND_CODEC_ERROR_UNKNOWN_COMMAND_TYPE;
    }

    header->type = type;
    header->sequence = sequence;

    // Ungeprueft uebernommen, auch RAD_USER_NONE: siehe codec.h.
    header->user = user;

    return RAD_COMMAND_CODEC_OK;
}

void RAD_SerializeCommand(RAD_ByteWriter_t *writer, const RAD_Command_t *command)
{
    RAD_SerializeCommandHeader(writer, &command->header);

    switch(command->header.type)
    {
        case RAD_COMMAND_TYPE_SPAWN_ENTITY:
            RAD_SerializeCommandSpawnEntity(writer, &command->command.spawn_entity);
            break;
        case RAD_COMMAND_TYPE_MOVE_ENTITY:
            RAD_SerializeCommandMoveEntity(writer, &command->command.move_entity);
            break;
        case RAD_COMMAND_TYPE_REMOVE_ENTITY:
            RAD_SerializeCommandRemoveEntity(writer, &command->command.remove_entity);
            break;
        case RAD_COMMAND_TYPE_CREATE_TILE:
            RAD_SerializeCommandCreateTile(writer, &command->command.create_tile);
            break;
        case RAD_COMMAND_TYPE_REMOVE_TILE:
            RAD_SerializeCommandRemoveTile(writer, &command->command.remove_tile);
            break;
        case RAD_COMMAND_TYPE_SHOOT:
            RAD_SerializeCommandShoot(writer, &command->command.shoot);
            break;
        case RAD_COMMAND_TYPE_USE:
            RAD_SerializeCommandUse(writer, &command->command.use);
            break;

        // Ohne Nutzlast: der Kopf ist schon die ganze Nachricht.
        case RAD_COMMAND_TYPE_END_TURN:
            break;

        // Ein nie gefuelltes Kommando: der Kopf traegt schon die reservierte 0,
        // eine Nutzlast gibt es nicht. Die Gegenseite lehnt das ab.
        case RAD_COMMAND_TYPE_NONE:
        default:
            break;
    }
}

RAD_CommandCodecResult_t RAD_DeserializeCommand(RAD_ByteReader_t *reader, RAD_Command_t *command)
{
    // In ein eigenes Kommando lesen und "command" erst am Ende setzen: bei jedem
    // Fehler -- auch dem der Nutzlast -- bleibt es unberuehrt.
    RAD_Command_t parsed = {0};

    RAD_CommandCodecResult_t result = RAD_DeserializeCommandHeader(reader, &parsed.header);
    if(result != RAD_COMMAND_CODEC_OK)
    {
        return result;
    }

    const RAD_CommandType_t type = parsed.header.type;

    switch(type)
    {
        case RAD_COMMAND_TYPE_SPAWN_ENTITY:
            result = RAD_DeserializeCommandSpawnEntity(reader, &parsed.command.spawn_entity);
            break;
        case RAD_COMMAND_TYPE_MOVE_ENTITY:
            result = RAD_DeserializeCommandMoveEntity(reader, &parsed.command.move_entity);
            break;
        case RAD_COMMAND_TYPE_REMOVE_ENTITY:
            result = RAD_DeserializeCommandRemoveEntity(reader, &parsed.command.remove_entity);
            break;
        case RAD_COMMAND_TYPE_CREATE_TILE:
            result = RAD_DeserializeCommandCreateTile(reader, &parsed.command.create_tile);
            break;
        case RAD_COMMAND_TYPE_REMOVE_TILE:
            result = RAD_DeserializeCommandRemoveTile(reader, &parsed.command.remove_tile);
            break;
        case RAD_COMMAND_TYPE_SHOOT:
            result = RAD_DeserializeCommandShoot(reader, &parsed.command.shoot);
            break;
        case RAD_COMMAND_TYPE_USE:
            result = RAD_DeserializeCommandUse(reader, &parsed.command.use);
            break;

        // Nichts zu lesen; "result" steht auf dem Ergebnis des Kopfes. Ob wirklich
        // nichts mehr dasteht, faengt die Pruefung auf Trailing-Bytes unten ab.
        case RAD_COMMAND_TYPE_END_TURN:
            break;

        // Unerreichbar: RAD_CommandTypeFromWire hat die Art schon bestaetigt, und
        // RAD_COMMAND_TYPE_NONE hat keine Wire-Nummer.
        case RAD_COMMAND_TYPE_NONE:
        default:
            return RAD_COMMAND_CODEC_ERROR_UNKNOWN_COMMAND_TYPE;
    }

    if(result != RAD_COMMAND_CODEC_OK)
    {
        return result;
    }

    if(RAD_ByteReaderRemaining(reader) != 0)
    {
        return RAD_COMMAND_CODEC_ERROR_TRAILING_BYTES;
    }

    *command = parsed;

    return RAD_COMMAND_CODEC_OK;
}
