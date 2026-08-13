#ifndef __RAD_COMMAND_REMOVE_TILE_H__
#define __RAD_COMMAND_REMOVE_TILE_H__

#include <radish/game/control/command/codec.h>

///
/// Nutzlast von RAD_CommandRemoveTile_t, 4 Byte:
///
///     x   2 Byte   int16
///     y   2 Byte   int16
///
/// Nur die Nutzlast -- den Kopf schreibt und liest RAD_SerializeCommand bzw.
/// RAD_DeserializeCommand.
///
/// Kein Tile-Typ, weil es nur einen Weg gibt, nichts zu sein -- das Kommando
/// selbst sagt schon alles. Ob (x,y) in der Welt liegt, entscheidet erst das
/// Ausfuehren.
///
void RAD_SerializeCommandRemoveTile(RAD_ByteWriter_t *writer, const RAD_CommandRemoveTile_t *command);
RAD_CommandCodecResult_t RAD_DeserializeCommandRemoveTile(RAD_ByteReader_t *reader, RAD_CommandRemoveTile_t *command);

#endif
