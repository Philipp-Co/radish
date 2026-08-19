#ifndef __RAD_COMMAND_SHOOT_H__
#define __RAD_COMMAND_SHOOT_H__

#include <radish/game/control/command/codec.h>

///
/// Nutzlast von RAD_CommandShoot_t, 9 Byte:
///
///     entity   4 Byte   int32, die RAD_EntityId_t des Schuetzen
///     x        2 Byte   int16
///     y        2 Byte   int16
///     weapon   1 Byte   uint8, ungeprueft
///
/// Nur die Nutzlast -- den Kopf schreibt und liest RAD_SerializeCommand bzw.
/// RAD_DeserializeCommand.
///
/// Die Waffe geht durch, wie sie ist, die 0 eingeschlossen: sie ist eine Nummer
/// und keine Auswahl aus bekannten Werten (siehe command.h). Waffen gibt es im
/// Spiel noch nicht -- gaebe es hier eine Pruefung, waere sie eine Liste, die
/// niemand fuehrt.
///
/// Die Id des Schuetzen geht ebenso ungeprueft durch, RAD_ENTITY_NONE (-1)
/// eingeschlossen -- wie bei move_entity ist jede Zahl eine lesbare Angabe. Ob es
/// die Figur gibt, ob sie dem Absender gehoert, ob (x,y) in der Welt liegt und ob
/// dort etwas zu treffen ist, entscheidet erst das Ausfuehren.
///
void RAD_SerializeCommandShoot(RAD_ByteWriter_t *writer, const RAD_CommandShoot_t *command);
RAD_CommandCodecResult_t RAD_DeserializeCommandShoot(RAD_ByteReader_t *reader, RAD_CommandShoot_t *command);

#endif
