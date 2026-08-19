#ifndef __RAD_COMMAND_USE_H__
#define __RAD_COMMAND_USE_H__

#include <radish/game/control/command/codec.h>

///
/// Nutzlast von RAD_CommandUse_t, 8 Byte:
///
///     entity   4 Byte   int32, die RAD_EntityId_t dessen, der benutzt
///     x        2 Byte   int16
///     y        2 Byte   int16
///
/// Nur die Nutzlast -- den Kopf schreibt und liest RAD_SerializeCommand bzw.
/// RAD_DeserializeCommand.
///
/// Alle drei Felder sind Zahlen, jede Bytefolge der richtigen Laenge ist damit ein
/// lesbares Kommando. Ob es die Figur gibt, ob sie dem Absender gehoert, ob (x,y)
/// in der Welt liegt und ob dort etwas zu benutzen ist, entscheidet erst das
/// Ausfuehren.
///
void RAD_SerializeCommandUse(RAD_ByteWriter_t *writer, const RAD_CommandUse_t *command);
RAD_CommandCodecResult_t RAD_DeserializeCommandUse(RAD_ByteReader_t *reader, RAD_CommandUse_t *command);

#endif
