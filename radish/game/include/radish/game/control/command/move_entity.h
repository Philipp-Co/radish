#ifndef __RAD_COMMAND_MOVE_ENTITY_H__
#define __RAD_COMMAND_MOVE_ENTITY_H__

#include <radish/game/control/command/codec.h>

///
/// Nutzlast von RAD_CommandMoveEntity_t, 12 Byte:
///
///     entity   4 Byte   int32, die RAD_EntityId_t
///     from_x   2 Byte   int16
///     from_y   2 Byte   int16
///     to_x     2 Byte   int16
///     to_y     2 Byte   int16
///
/// Nur die Nutzlast -- den Kopf schreibt und liest RAD_SerializeCommand bzw.
/// RAD_DeserializeCommand.
///
/// Hier kann nichts unbekannt sein: alle fuenf Felder sind Zahlen, jede Bytefolge
/// der richtigen Laenge ist damit ein lesbares Kommando. Ob die Id auf eine
/// Entitaet zeigt, ob sie wirklich auf (from_x,from_y) steht und ob das Ziel frei
/// ist, entscheidet erst das Ausfuehren. RAD_ENTITY_NONE (-1) wird deshalb auch
/// nicht abgewiesen; es ist eine gueltige Zahl und beim Ausfuehren ein
/// Fehlschlag.
///
void RAD_SerializeCommandMoveEntity(RAD_ByteWriter_t *writer, const RAD_CommandMoveEntity_t *command);
RAD_CommandCodecResult_t RAD_DeserializeCommandMoveEntity(RAD_ByteReader_t *reader, RAD_CommandMoveEntity_t *command);

#endif
