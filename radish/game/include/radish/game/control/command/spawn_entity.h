#ifndef __RAD_COMMAND_SPAWN_ENTITY_H__
#define __RAD_COMMAND_SPAWN_ENTITY_H__

#include <radish/game/control/command/codec.h>

///
/// Nutzlast von RAD_CommandSpawnEntity_t, 6 Byte:
///
///     entity_type   1 Byte   Wire-Nummer, siehe codec.h
///     x             2 Byte   int16
///     y             2 Byte   int16
///     z             1 Byte   int8
///
/// Nur die Nutzlast -- den Kopf schreibt und liest RAD_SerializeCommand bzw.
/// RAD_DeserializeCommand.
///
/// Geprueft wird nur, was in diesen sechs Byte selbst nicht stimmen kann: eine
/// unbekannte Wire-Nummer des Entitaetstyps. Ob (x,y) in der Welt liegt und ob
/// das Tile frei ist, entscheidet erst das Ausfuehren -- so wie der
/// Tile-Serialisierer im Speicherformat nichts ueber sein Tile hinaus prueft.
///
void RAD_SerializeCommandSpawnEntity(RAD_ByteWriter_t *writer, const RAD_CommandSpawnEntity_t *command);
RAD_CommandCodecResult_t RAD_DeserializeCommandSpawnEntity(RAD_ByteReader_t *reader, RAD_CommandSpawnEntity_t *command);

#endif
