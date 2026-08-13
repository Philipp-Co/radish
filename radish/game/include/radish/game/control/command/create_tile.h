#ifndef __RAD_COMMAND_CREATE_TILE_H__
#define __RAD_COMMAND_CREATE_TILE_H__

#include <radish/game/control/command/codec.h>

///
/// Nutzlast von RAD_CommandCreateTile_t, 6 Byte:
///
///     tile_type   1 Byte   Wire-Nummer, siehe codec.h
///     x           2 Byte   int16
///     y           2 Byte   int16
///     z           1 Byte   int8
///
/// Nur die Nutzlast -- den Kopf schreibt und liest RAD_SerializeCommand bzw.
/// RAD_DeserializeCommand.
///
/// RAD_TILE_TYPE_VOID hat eine eigene Wire-Nummer und wird nicht abgewiesen: void
/// ist ein Zustand des Gelaendes und keine fehlende Angabe. Dass es fuer "Gelaende
/// wegnehmen" auch RAD_COMMAND_TYPE_REMOVE_TILE gibt, macht das Kommando hier
/// nicht ungueltig -- beide Wege fuehren zum selben Zustand, und das zu bewerten
/// ist nicht Sache des Formats.
///
/// Ob (x,y) in der Welt liegt, entscheidet erst das Ausfuehren.
///
void RAD_SerializeCommandCreateTile(RAD_ByteWriter_t *writer, const RAD_CommandCreateTile_t *command);
RAD_CommandCodecResult_t RAD_DeserializeCommandCreateTile(RAD_ByteReader_t *reader, RAD_CommandCreateTile_t *command);

#endif
