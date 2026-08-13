#ifndef __RAD_COMMAND_REMOVE_ENTITY_H__
#define __RAD_COMMAND_REMOVE_ENTITY_H__

#include <radish/game/control/command/codec.h>

///
/// Nutzlast von RAD_CommandRemoveEntity_t, 4 Byte:
///
///     entity   4 Byte   int32, die RAD_EntityId_t
///
/// Nur die Nutzlast -- den Kopf schreibt und liest RAD_SerializeCommand bzw.
/// RAD_DeserializeCommand.
///
/// Ob die Id auf eine Entitaet zeigt, entscheidet erst das Ausfuehren.
///
void RAD_SerializeCommandRemoveEntity(RAD_ByteWriter_t *writer, const RAD_CommandRemoveEntity_t *command);
RAD_CommandCodecResult_t RAD_DeserializeCommandRemoveEntity(RAD_ByteReader_t *reader, RAD_CommandRemoveEntity_t *command);

#endif
