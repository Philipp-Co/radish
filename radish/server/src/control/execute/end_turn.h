#ifndef __RAD_CONTROL_EXECUTE_END_TURN_H__
#define __RAD_CONTROL_EXECUTE_END_TURN_H__

#include <stdint.h>
#include <radish/game/game.h>
#include <radish/game/user.h>
#include <radish/game/control/command/command.h>

///
/// control/execute/end_turn -- der Zug, abgegeben.
///
/// Privat wie move: der Header liegt neben seiner Quelle. Von aussen fuehrt genau
/// ein Weg in die Steuerung (RAD_ControlExecuteCommand), und der entscheidet
/// vorher, ob das Kommando ueberhaupt gilt.
///
/// Es gibt zwei Wege, einen Zug zu beenden, und das ist der ausdrueckliche: der
/// Spieler gibt ab, was er noch haette. Der andere braucht kein Kommando -- wer
/// seine Aktionspunkte aufgebraucht hat, ist fertig, und execute.c schaltet
/// weiter (siehe execute.h).
///

///
/// Fuehrt ein RAD_COMMAND_TYPE_END_TURN aus.
///
/// Liefert den Wert, der in die Antwort geht: ein RAD_ControlResult_t, deshalb
/// uint32_t -- wie bei move.
///
/// "command" traegt ausser dem Kopf nichts, wird aber trotzdem uebergeben: die
/// Ausfuehrenden haben alle dieselbe Form, und wer sie ruft, muss nicht wissen,
/// welche davon eine Nutzlast liest.
///
uint32_t RAD_ControlExecuteEndTurnCommand(const RAD_Command_t *command, RAD_Game_t *game, RAD_UserId_t user);

#endif
