#include "end_turn.h"

#include <radish/server/control/execute.h>

///
/// Weitergeschaltet wird ueber das Spiel und nicht ueber den Zug selbst
/// (RAD_TurnEnd): dieselbe Ueberlegung wie bei move und der Welt -- der Weg nach
/// draussen ist die Fassade, und deren Ergebnisse sind schon die des Spiels.
///
/// Dass der Absender dran ist, hat RAD_ControlCheckCommand bereits entschieden.
/// Die Ablehnungen unten sind trotzdem ausgeschrieben: was hier zurueckkaeme,
/// waere ein Widerspruch zwischen Pruefung und Ausfuehrung, und der soll in der
/// Antwort stehen und nicht als RAD_CONTROL_OK durchgehen.
///

uint32_t RAD_ControlExecuteEndTurnCommand(const RAD_Command_t *command, RAD_Game_t *game, RAD_UserId_t user)
{
    (void)command;

    switch(RAD_GameEndTurn(game, user))
    {
        case RAD_GAME_OK:
            return (uint32_t)RAD_CONTROL_OK;

        case RAD_GAME_ERROR_NO_USER:
            return (uint32_t)RAD_CONTROL_ERROR_NO_USER;

        case RAD_GAME_ERROR_NOT_YOUR_TURN:
            return (uint32_t)RAD_CONTROL_ERROR_NOT_YOUR_TURN;

        case RAD_GAME_ERROR_NOT_PLAYING:
        default:
            return (uint32_t)RAD_CONTROL_ERROR_NOT_PLAYING;
    }
}
