#ifndef __RAD_CONTROL_EXECUTE_MOVE_H__
#define __RAD_CONTROL_EXECUTE_MOVE_H__

#include <stdint.h>
#include <radish/game/game.h>
#include <radish/game/user.h>
#include <radish/game/control/command/command.h>

///
/// control/execute/move -- der Zug einer Figur, ausgefuehrt.
///
/// Privat wie session/: der Header liegt neben seiner Quelle und nicht im
/// Include-Verzeichnis. Von aussen fuehrt genau ein Weg in die Steuerung
/// (RAD_ControlExecuteCommand), und der entscheidet vorher, ob das Kommando
/// ueberhaupt gilt. Waere dieses Modul oeffentlich, liesse sich ein Zug an der
/// Pruefung vorbei ausfuehren.
///
/// Die Aufteilung dahinter: execute.c entscheidet und verteilt, je eine Datei
/// hier fuehrt eine Kommandoart aus. Eine Kommandoart bringt Spielregeln mit --
/// wohin darf gezogen werden, was passiert mit dem Ziel --, und die gehoeren nicht
/// in die Verteilung.
///

///
/// Fuehrt ein RAD_COMMAND_TYPE_MOVE_ENTITY aus.
///
/// Liefert den Wert, der in die Antwort geht: ein RAD_ControlResult_t, deshalb
/// uint32_t -- so wie "value" es traegt (siehe execute.h).
///
/// "command" ist const: der Zug steht danach in der Welt, nicht im Kommando.
/// Aufgerufen wird nur mit einem Kommando dieser Art und erst, wenn feststeht,
/// dass der Benutzer es ausfuehren darf; geprueft wird hier nur noch, was das
/// Spiel selbst dazu sagt.
///
/// "user" ist der, in dessen Namen gezogen wird. Fuer den Zug selbst spielt er
/// heute keine Rolle -- ein Feld ist ein Feld, und wem die Figur gehoert, ist
/// schon entschieden. Er steht trotzdem in der Schnittstelle, weil jede
/// Ausfuehrung im Namen eines Benutzers geschieht: sobald es Regeln gibt, die an
/// ihm haengen (Bewegungspunkte, Zugreihenfolge, Sichtweite), liegt er hier schon
/// vor, ohne dass sich der Aufruf aendert.
///
uint32_t RAD_ControlExecuteMoveCommand(const RAD_Command_t *command, RAD_Game_t *game, RAD_UserId_t user);

#endif
