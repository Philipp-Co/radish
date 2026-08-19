#include <radish/server/control/execute.h>

#include "execute/move.h"
#include "execute/end_turn.h"

#include <stdlib.h>
#include <stddef.h>

///
/// Die Teilnehmerliste fuehrt das Spiel selbst; sie ist dort die Reihenfolge des
/// Zuges (radish/game/model/turn/turn.h). Diese Datei haelt davon nichts mehr,
/// sondern reicht durch: RAD_ControlAddUser und die anderen sind Weiterleitungen,
/// die das Ergebnis der Regeln auf das des Protokolls abbilden -- dieselbe
/// Uebersetzung, die vorher zwischen der eigenen Teilnehmerliste und
/// RAD_ControlResult_t stand.
///
/// Sie bleibt trotzdem der Weg nach draussen: wer mitspielt, aendert man ueber
/// diese Funktionen und nicht am Spiel vorbei, damit "ob ein Kommando gilt" an
/// einer Stelle entschieden wird.
///
/// Zwei Schritte: RAD_ControlCheckCommand entscheidet, ob das Kommando gilt, und
/// erst danach fuehrt RAD_ControlExecuteAllowedCommand es aus -- ueber je eine
/// Datei unter execute/, eine je Kommandoart. Diese Datei kennt damit keine
/// Spielregel, und die Ausfuehrenden keine Berechtigung.
///

struct RAD_Control
{
    /// Geliehen, nicht uebernommen -- RAD_DestroyControl baut es nicht ab.
    RAD_Game_t *game;
};

static RAD_ControlResult_t RAD_ControlCheckCommand(RAD_Control_t control, const RAD_Command_t *command);
static RAD_ControlResult_t RAD_ControlCheckEntityOwner(RAD_Control_t control, RAD_UserId_t user, RAD_EntityId_t entity);
static uint32_t RAD_ControlExecuteAllowedCommand(RAD_Control_t control, const RAD_Command_t *command);
static void RAD_ControlBookExecutedCommand(RAD_Control_t control, const RAD_Command_t *command);
static int32_t RAD_ControlCommandCost(RAD_CommandType_t type);


const char* RAD_ControlResultText(RAD_ControlResult_t result)
{
    switch(result)
    {
        case RAD_CONTROL_OK:                    return "in Ordnung";
        case RAD_CONTROL_ERROR_NO_USER:         return "Kommando ohne Absender";
        case RAD_CONTROL_ERROR_NOT_PLAYING:     return "Absender spielt nicht mit";
        case RAD_CONTROL_ERROR_NOT_OWNED:       return "Figur gehoert einem anderen";
        case RAD_CONTROL_ERROR_NOT_EXECUTED:    return "in Ordnung, aber noch nicht ausgefuehrt";
        case RAD_CONTROL_ERROR_NO_ENTITY:       return "Aufruf ohne Figur";
        case RAD_CONTROL_ERROR_NO_SUCH_ENTITY:  return "Figur steht nicht in der Welt";
        case RAD_CONTROL_ERROR_OUT_OF_BOUNDS:   return "Ziel liegt ausserhalb der Welt";
        case RAD_CONTROL_ERROR_TARGET_OCCUPIED: return "Zielfeld ist besetzt";
        case RAD_CONTROL_ERROR_NOT_YOUR_TURN:   return "ein anderer ist dran";
        case RAD_CONTROL_ERROR_NOT_ENOUGH_ACTION_POINTS:
                                                return "nicht genug Aktionspunkte";
        default:                                return "unbekanntes Ergebnis";
    }
}

RAD_Control_t RAD_CreateControl(RAD_Game_t *game)
{
    RAD_Control_t control = malloc(sizeof(struct RAD_Control));
    if(control == NULL)
    {
        return NULL;
    }

    control->game = game;

    return control;
}

void RAD_DestroyControl(RAD_Control_t *control)
{
    free(*control);
    *control = NULL;
}

RAD_ControlResult_t RAD_ControlAddUser(RAD_Control_t control, RAD_UserId_t user)
{
    // Schon aufgenommen ist derselbe Zustand wie eben aufgenommen und kommt
    // deshalb schon als RAD_GAME_OK zurueck.
    switch(RAD_GameAddPlayer(control->game, user))
    {
        case RAD_GAME_OK:
            return RAD_CONTROL_OK;

        case RAD_GAME_ERROR_NO_USER:
            return RAD_CONTROL_ERROR_NO_USER;

        // Kein Platz mehr frei -- der Name sagt, was danach gilt.
        case RAD_GAME_ERROR_FULL:
        default:
            return RAD_CONTROL_ERROR_NOT_PLAYING;
    }
}

void RAD_ControlRemoveUser(RAD_Control_t control, RAD_UserId_t user)
{
    RAD_GameRemovePlayer(control->game, user);
}

RAD_ControlResult_t RAD_ControlBindUserEntity(RAD_Control_t control, RAD_UserId_t user, RAD_EntityId_t entity)
{
    switch(RAD_GameBindEntity(control->game, user, entity))
    {
        case RAD_GAME_OK:
            return RAD_CONTROL_OK;

        case RAD_GAME_ERROR_NO_ENTITY:
            return RAD_CONTROL_ERROR_NO_ENTITY;

        case RAD_GAME_ERROR_NOT_OWNED:
            return RAD_CONTROL_ERROR_NOT_OWNED;

        // Ohne Absender ist niemand da, der mitspielen koennte -- fuer den
        // Aufrufer ist das dasselbe wie ein Benutzer, der es nicht tut.
        case RAD_GAME_ERROR_NO_USER:
        case RAD_GAME_ERROR_NOT_PLAYING:
        default:
            return RAD_CONTROL_ERROR_NOT_PLAYING;
    }
}

void RAD_ControlUnbindEntity(RAD_Control_t control, RAD_EntityId_t entity)
{
    RAD_GameUnbindEntity(control->game, entity);
}

int32_t RAD_ControlNumberOfUserEntities(RAD_Control_t control, RAD_UserId_t user)
{
    return RAD_GameNumberOfUserEntities(control->game, user);
}

RAD_EntityId_t RAD_ControlUserEntityAt(RAD_Control_t control, RAD_UserId_t user, int32_t index)
{
    return RAD_GameUserEntityAt(control->game, user, index);
}

RAD_UserId_t RAD_ControlEntityOwner(RAD_Control_t control, RAD_EntityId_t entity)
{
    return RAD_GameEntityOwner(control->game, entity);
}

int32_t RAD_ControlNumberOfPlayers(RAD_Control_t control)
{
    return RAD_GameNumberOfPlayers(control->game);
}

RAD_CommandResponse_t RAD_ControlExecuteCommand(RAD_Control_t control, const RAD_Command_t *command)
{
    const RAD_ControlResult_t allowed = RAD_ControlCheckCommand(control, command);

    // Spricht etwas dagegen, ist das schon die Antwort -- ausgefuehrt wird dann
    // nichts, und der Spielzustand bleibt unberuehrt.
    const uint32_t value = (allowed == RAD_CONTROL_OK)
        ? RAD_ControlExecuteAllowedCommand(control, command)
        : (uint32_t)allowed;

    // Erst jetzt wird abgerechnet, und nur, was auch geschehen ist: ein
    // abgelehntes Kommando und eines ohne Ausfuehrenden lassen den Zustand
    // unberuehrt und duerfen deshalb auch keinen Zug kosten.
    if(value == (uint32_t)RAD_CONTROL_OK)
    {
        RAD_ControlBookExecutedCommand(control, command);
    }

    // Kopf und Kommando aus derselben Quelle: damit tragen beide Koepfe dasselbe,
    // und das Kommando geht als genaue Kopie zurueck.
    RAD_CommandResponse_t response = {
        .header = command->header,
        .value = value,
        .command = *command
    };

    return response;
}

///
/// Verteilt auf die Ausfuehrenden unter execute/. Gerufen wird das erst, wenn
/// feststeht, dass der Absender das Kommando ausfuehren darf.
///
static uint32_t RAD_ControlExecuteAllowedCommand(RAD_Control_t control, const RAD_Command_t *command)
{
    switch(command->header.type)
    {
        case RAD_COMMAND_TYPE_MOVE_ENTITY:
            return RAD_ControlExecuteMoveCommand(command, control->game, command->header.user);

        case RAD_COMMAND_TYPE_END_TURN:
            return RAD_ControlExecuteEndTurnCommand(command, control->game, command->header.user);

        // Fuer diese Arten gibt es noch keinen Ausfuehrenden. Sie kommen einzeln
        // dazu, jede als eigene Datei unter execute/.
        case RAD_COMMAND_TYPE_SPAWN_ENTITY:
        case RAD_COMMAND_TYPE_REMOVE_ENTITY:
        case RAD_COMMAND_TYPE_CREATE_TILE:
        case RAD_COMMAND_TYPE_REMOVE_TILE:
        case RAD_COMMAND_TYPE_SHOOT:
        case RAD_COMMAND_TYPE_USE:
            return (uint32_t)RAD_CONTROL_ERROR_NOT_EXECUTED;

        // Unerreichbar: aus dem Codec kommt kein Kommando ohne Art.
        case RAD_COMMAND_TYPE_NONE:
        default:
            return (uint32_t)RAD_CONTROL_ERROR_NOT_EXECUTED;
    }
}

///
/// Liefert RAD_CONTROL_OK, wenn nichts gegen das Kommando spricht.
///
static RAD_ControlResult_t RAD_ControlCheckCommand(RAD_Control_t control, const RAD_Command_t *command)
{
    if(command->header.user == RAD_USER_NONE)
    {
        return RAD_CONTROL_ERROR_NO_USER;
    }

    // Nicht aufgenommen, nur nachgefragt: wer beitritt, entscheidet der, bei dem
    // die Nachrichten ankommen (RAD_ControlAddUser).
    if(!RAD_GameIsPlaying(control->game, command->header.user))
    {
        return RAD_CONTROL_ERROR_NOT_PLAYING;
    }

    // Ein Kommando ist ein Zug, und ziehen darf nur, wer dran ist -- das gilt fuer
    // jede Art, das Abgeben eingeschlossen.
    if(!RAD_GameIsUsersTurn(control->game, command->header.user))
    {
        return RAD_CONTROL_ERROR_NOT_YOUR_TURN;
    }

    // Und nur, was er sich leisten kann. Solange jedes Kommando einen Punkt
    // kostet, kann das nicht eintreten: wer dran ist, hat immer mindestens einen
    // -- bei null endet sein Zug von selbst. Die Pruefung steht trotzdem hier,
    // damit ein teureres Kommando nicht erst nach dem Ausfuehren auffaellt.
    if(!RAD_TurnCanSpendActionPoints(&control->game->turn,
                                     command->header.user,
                                     RAD_ControlCommandCost(command->header.type)))
    {
        return RAD_CONTROL_ERROR_NOT_ENOUGH_ACTION_POINTS;
    }

    switch(command->header.type)
    {
        case RAD_COMMAND_TYPE_MOVE_ENTITY:
            return RAD_ControlCheckEntityOwner(control, command->header.user, command->command.move_entity.entity);

        case RAD_COMMAND_TYPE_REMOVE_ENTITY:
            return RAD_ControlCheckEntityOwner(control, command->header.user, command->command.remove_entity.entity);

        // Nicht das Ziel wird geprueft, sondern der, der handelt: geschossen und
        // benutzt wird auf ein Feld, und was dort steht, gehoert gerade nicht dem
        // Absender -- sonst haette ein Schuss wenig Sinn.
        case RAD_COMMAND_TYPE_SHOOT:
            return RAD_ControlCheckEntityOwner(control, command->header.user, command->command.shoot.entity);

        case RAD_COMMAND_TYPE_USE:
            return RAD_ControlCheckEntityOwner(control, command->header.user, command->command.use.entity);

        // Kein Besitz zu pruefen: diese Kommandos fassen keine vorhandene Figur
        // an. Wer eine setzen und wer Gelaende legen darf, ist eine eigene Frage
        // und noch offen -- heute darf es jeder, der dran ist. Und wer abgibt,
        // fasst gar nichts an.
        case RAD_COMMAND_TYPE_SPAWN_ENTITY:
        case RAD_COMMAND_TYPE_CREATE_TILE:
        case RAD_COMMAND_TYPE_REMOVE_TILE:
        case RAD_COMMAND_TYPE_END_TURN:
            break;

        // Unerreichbar: aus dem Codec kommt kein Kommando ohne Art.
        case RAD_COMMAND_TYPE_NONE:
        default:
            break;
    }

    return RAD_CONTROL_OK;
}

///
/// Rechnet ein ausgefuehrtes Kommando gegen den Zug ab: bezahlen und, wenn danach
/// nichts mehr uebrig ist, weiterschalten.
///
/// Beides erst hinterher und nicht schon beim Ausfuehren: was ein Kommando
/// bewirkt, ist Sache des Spiels, was es kostet, eine des Ablaufs -- und der
/// Ausfuehrende unter execute/ soll nicht wissen muessen, dass es Aktionspunkte
/// gibt. Gerufen wird nur nach RAD_CONTROL_OK, also nur, wenn der Zustand sich
/// wirklich geaendert hat.
///
/// Das Abbuchen kann nicht fehlschlagen: dass der Absender dran ist und genug
/// hat, steht seit RAD_ControlCheckCommand fest, und dazwischen liegt nur die
/// Ausfuehrung, die den Zug nicht anfasst. Nur end_turn tut es -- deshalb kostet
/// es nichts, und deshalb fragt die zweite Haelfte noch einmal nach, wer jetzt
/// dran ist: nach einem abgegebenen Zug ist es schon der naechste, und dessen
/// voller Vorrat waere sonst der Anlass, ihn gleich wieder weiterzureichen.
///
static void RAD_ControlBookExecutedCommand(RAD_Control_t control, const RAD_Command_t *command)
{
    RAD_Turn_t *turn = &control->game->turn;
    const RAD_UserId_t user = command->header.user;

    RAD_TurnSpendActionPoints(turn, user, RAD_ControlCommandCost(command->header.type));

    if(RAD_TurnIsUsersTurn(turn, user) && (RAD_TurnActionPoints(turn) == 0))
    {
        RAD_GameEndTurn(control->game, user);
    }
}

///
/// Was ein Kommando an Aktionspunkten kostet.
///
/// Alles, was den Spielzustand aendert, kostet einen; abgeben kostet nichts, sonst
/// waere es ein Zug fuer sich. Die Zahlen stehen an dieser einen Stelle und nicht
/// bei den Ausfuehrenden: was eine Handlung wert ist, ist eine Frage des Spiels
/// und wird sich aendern -- dass sie ueberhaupt etwas kostet, nicht.
///
static int32_t RAD_ControlCommandCost(RAD_CommandType_t type)
{
    switch(type)
    {
        case RAD_COMMAND_TYPE_SPAWN_ENTITY:
        case RAD_COMMAND_TYPE_MOVE_ENTITY:
        case RAD_COMMAND_TYPE_REMOVE_ENTITY:
        case RAD_COMMAND_TYPE_CREATE_TILE:
        case RAD_COMMAND_TYPE_REMOVE_TILE:
        case RAD_COMMAND_TYPE_SHOOT:
        case RAD_COMMAND_TYPE_USE:
            return 1;

        case RAD_COMMAND_TYPE_END_TURN:
            return 0;

        // Unerreichbar: aus dem Codec kommt kein Kommando ohne Art.
        case RAD_COMMAND_TYPE_NONE:
        default:
            return 0;
    }
}

///
/// Darf dieser Mitspieler diese Figur anfassen? Die Regel dazu steht im Spiel
/// (RAD_GameMayControlEntity), hier wird sie nur auf eine Antwort abgebildet, die
/// ueber die Strecke geht.
///
static RAD_ControlResult_t RAD_ControlCheckEntityOwner(RAD_Control_t control, RAD_UserId_t user, RAD_EntityId_t entity)
{
    if(!RAD_GameMayControlEntity(control->game, user, entity))
    {
        return RAD_CONTROL_ERROR_NOT_OWNED;
    }

    return RAD_CONTROL_OK;
}
