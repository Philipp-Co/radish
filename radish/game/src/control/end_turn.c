#include <radish/game/game.h>
#include <radish/game/model/game.h>
#include <radish/game/control/commands.h>
#include <radish/game/control/unit_turn.h>

///
/// Das Abgeben des Zuges (RAD_COMMAND_TYPE_END_TURN): die Ausfuehrung und ihr
/// Handler. Eine eigene Regel gibt es nicht -- wer abgeben darf, entscheidet der
/// Zug (RAD_TurnEnd, model/turn.c) --, und eine Fabrik auch nicht: das Kommando
/// hat keine Nutzlast, wer abgibt, steht im Kopf.
///

RAD_GameResult_t RAD_GameEndTurn(RAD_Game_t *game, RAD_UserId_t user)
{
    // Die Regel steht im Zug; hier wird nur uebersetzt. "Steht nicht in der
    // Reihe" heisst nach aussen "spielt nicht mit": wer mitspielt, steht in der
    // Reihe -- dafuer sorgen RAD_GameAddPlayer und RAD_GameRemovePlayer.
    if(game->finished)
    {
        return RAD_GAME_ERROR_FINISHED;
    }

    switch(RAD_TurnEnd(&game->turn, user))
    {
        case RAD_TURN_OK:
            game->started = true;
            // War das die letzte Runde, ist das Spiel vorbei, und ein neuer Zug
            // beginnt nicht mehr: gemeldet wird nur das Ende (control/victory.c).
            if(RAD_GameFinishAfterLastRound(game))
            {
                return RAD_GAME_OK;
            }
            // Immer, auch wenn derselbe wieder dran ist: es ist ein neuer Zug.
            RAD_GameStartTurn(game);
            RAD_EventManagerPublishTurnChanged(game->event_manager, RAD_TurnCurrentUser(&game->turn));
            return RAD_GAME_OK;

        case RAD_TURN_ERROR_NO_USER:
            return RAD_GAME_ERROR_NO_USER;

        case RAD_TURN_ERROR_NOT_YOUR_TURN:
            return RAD_GAME_ERROR_NOT_YOUR_TURN;

        case RAD_TURN_ERROR_NOT_IN_ORDER:
        default:
            return RAD_GAME_ERROR_NOT_PLAYING;
    }
}

void RAD_GameHandleEndTurnCommand(RAD_Game_t *game, RAD_UserId_t user)
{
    // Wie ein Zug, der nicht geht: wer nicht dran ist, gibt still nichts ab.
    RAD_GameEndTurn(game, user);
}
