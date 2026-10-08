#include <radish/server/control/execute.h>

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
/// erst danach gibt RAD_ControlExecuteAllowedCommand es ins Spiel. Diese Datei
/// kennt damit keine Spielregel -- sie beantwortet, wer senden darf, und nicht, was
/// das Senden bewirkt.
///
/// **Ausgefuehrt wird an einer Stelle, und die liegt nicht hier.** Vorher lag unter
/// execute/ je eine Datei pro Kommandoart, die in der Welt nachsah und selbst
/// schrieb. Das ist weg: der Weg hinein ist RAD_GameExecuteCommand, und was ein
/// Kommando bewirkt und wann ein Zug vorbei ist, wertet das Spiel selbst aus.
///
/// Der Preis dafuer steht offen und ist an RAD_ControlExecuteAllowedCommand
/// beschrieben: aus dem Spiel kommt kein Ergebnis zurueck, also sagt "value" in der
/// Antwort vorlaeufig nur, dass das Kommando angenommen wurde.
///

struct RAD_Control
{
    /// Geliehen, nicht uebernommen -- RAD_DestroyControl baut es nicht ab.
    RAD_Game_t *game;
};

static RAD_ControlResult_t RAD_ControlCheckCommand(RAD_Control_t control, const RAD_Command_t *command);
static RAD_ControlResult_t RAD_ControlCheckUnitOwner(RAD_Control_t control, RAD_UserId_t user, RAD_UnitId_t unit);
static RAD_ControlResult_t RAD_ControlCheckDeploy(RAD_Control_t control, RAD_UserId_t user, const RAD_CommandDeployUnit_t *deploy);
static RAD_ControlResult_t RAD_ControlCheckMove(RAD_Control_t control, RAD_UserId_t user, const RAD_CommandMoveUnit_t *move);
static RAD_ControlResult_t RAD_ControlCheckAttack(RAD_Control_t control, RAD_UserId_t user, const RAD_CommandAttack_t *attack);
static RAD_ControlResult_t RAD_ControlFromUnitActionResult(RAD_GameResult_t result);
static uint32_t RAD_ControlExecuteAllowedCommand(RAD_Control_t control, const RAD_Command_t *command);


const char* RAD_ControlResultText(RAD_ControlResult_t result)
{
    switch(result)
    {
        case RAD_CONTROL_OK:                    return "in Ordnung";
        case RAD_CONTROL_ERROR_NO_USER:         return "Kommando ohne Absender";
        case RAD_CONTROL_ERROR_NOT_PLAYING:     return "Absender spielt nicht mit";
        case RAD_CONTROL_ERROR_NOT_OWNED:       return "Figur gehoert einem anderen";
        case RAD_CONTROL_ERROR_NOT_EXECUTED:    return "in Ordnung, aber noch nicht ausgefuehrt";
        case RAD_CONTROL_ERROR_NO_UNIT:         return "Aufruf ohne Figur";
        case RAD_CONTROL_ERROR_NO_SUCH_UNIT:    return "Figur steht nicht in der Welt";
        case RAD_CONTROL_ERROR_OUT_OF_BOUNDS:   return "Ziel liegt ausserhalb der Welt";
        case RAD_CONTROL_ERROR_TARGET_OCCUPIED: return "Zielfeld ist besetzt";
        case RAD_CONTROL_ERROR_NOT_YOUR_TURN:   return "ein anderer ist dran";
        case RAD_CONTROL_ERROR_NOT_IN_RESERVE:  return "Einheit steht nicht in der Reserve";
        case RAD_CONTROL_ERROR_NO_GROUND:       return "Zielfeld hat kein Gelaende";
        case RAD_CONTROL_ERROR_UNIT_JUST_DEPLOYED:
                                                return "Einheit ist in diesem Zug erst aufgestellt worden";
        case RAD_CONTROL_ERROR_UNIT_ALREADY_MOVED:
                                                return "Einheit ist in diesem Zug schon gezogen";
        case RAD_CONTROL_ERROR_UNIT_ALREADY_ATTACKED:
                                                return "Einheit hat in diesem Zug schon angegriffen";
        case RAD_CONTROL_ERROR_UNIT_NOT_DEPLOYED:
                                                return "Einheit steht nicht auf dem Feld";
        case RAD_CONTROL_ERROR_TARGET_OUT_OF_RANGE:
                                                return "Ziel liegt ausserhalb der Reichweite";
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

RAD_GameResult_t RAD_ControlStartGame(RAD_Control_t control, const RAD_GameStart_t *start)
{
    // Wie aus einem Spielstart ein Spiel wird, weiss das Spiel (start_game.h).
    return RAD_StartGame(control->game, start);
}

RAD_GameStartResult_t RAD_ControlStartGameFromFile(RAD_Control_t control, const char *path, RAD_GameResult_t *game_result)
{
    return RAD_StartGameFromFile(control->game, path, game_result);
}

void RAD_ControlRemoveUser(RAD_Control_t control, RAD_UserId_t user)
{
    RAD_GameRemovePlayer(control->game, user);
}

RAD_ControlResult_t RAD_ControlBindUserUnit(RAD_Control_t control, RAD_UserId_t user, RAD_UnitId_t unit)
{
    switch(RAD_GameBindUnit(control->game, user, unit))
    {
        case RAD_GAME_OK:
            return RAD_CONTROL_OK;

        case RAD_GAME_ERROR_NO_UNIT:
            return RAD_CONTROL_ERROR_NO_UNIT;

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

void RAD_ControlUnbindUnit(RAD_Control_t control, RAD_UnitId_t unit)
{
    RAD_GameUnbindUnit(control->game, unit);
}

int32_t RAD_ControlNumberOfUserUnits(RAD_Control_t control, RAD_UserId_t user)
{
    return RAD_GameNumberOfUserUnits(control->game, user);
}

RAD_UnitId_t RAD_ControlUserUnitAt(RAD_Control_t control, RAD_UserId_t user, int32_t index)
{
    return RAD_GameUserUnitAt(control->game, user, index);
}

int32_t RAD_ControlNumberOfUnits(RAD_Control_t control)
{
    return RAD_GameNumberOfUnits(control->game);
}

bool RAD_ControlUnitAt(RAD_Control_t control, int32_t index, RAD_Unit_t *output)
{
    return RAD_GameUnitAt(control->game, index, output);
}

RAD_UserId_t RAD_ControlUnitOwner(RAD_Control_t control, RAD_UnitId_t unit)
{
    return RAD_GameUnitOwner(control->game, unit);
}

int32_t RAD_ControlNumberOfPlayers(RAD_Control_t control)
{
    return RAD_GameNumberOfPlayers(control->game);
}

RAD_UserId_t RAD_ControlCurrentUser(RAD_Control_t control)
{
    return RAD_GameCurrentUser(control->game);
}

RAD_UserId_t RAD_ControlPlayerAt(RAD_Control_t control, int32_t index)
{
    return RAD_GamePlayerAt(control->game, index);
}

int32_t RAD_ControlWorldWidth(RAD_Control_t control)
{
    return RAD_GameWorldWidth(control->game);
}

int32_t RAD_ControlWorldHeight(RAD_Control_t control)
{
    return RAD_GameWorldHeight(control->game);
}

bool RAD_ControlTileAt(RAD_Control_t control, int32_t x, int32_t y, RAD_Tile_t *output)
{
    if(output == NULL)
    {
        return false;
    }

    // Die Grenze aus dem Spiel, nicht aus den Konstanten: die Welt kann kleiner
    // sein als die groesste (RAD_LoadWorldFromFile). Damit ist auch der Schritt
    // auf int16_t unten sicher -- eine Welt ist nie breiter als RAD_WORLD_WIDTH.
    if((x < 0) || (x >= RAD_GameWorldWidth(control->game)) ||
       (y < 0) || (y >= RAD_GameWorldHeight(control->game)))
    {
        return false;
    }

    return RAD_GameTileAt(control->game, (int16_t)x, (int16_t)y, output);
}

RAD_CommandResponse_t RAD_ControlExecuteCommand(RAD_Control_t control, const RAD_Command_t *command)
{
    const RAD_ControlResult_t allowed = RAD_ControlCheckCommand(control, command);

    // Spricht etwas dagegen, ist das schon die Antwort -- ausgefuehrt wird dann
    // nichts, und der Spielzustand bleibt unberuehrt.
    const uint32_t value = (allowed == RAD_CONTROL_OK)
        ? RAD_ControlExecuteAllowedCommand(control, command)
        : (uint32_t)allowed;

    // Abgerechnet wird hier nichts mehr. Bezahlen und Weiterschalten geschahen
    // vorher in dieser Datei, hinterher und nur bei RAD_CONTROL_OK -- jetzt tut es
    // das Spiel, in demselben Durchgang, in dem es das Kommando ausfuehrt. Damit
    // gibt es keinen Zeitpunkt mehr, in dem ein Zustand geaendert und noch nicht
    // abgerechnet ist.

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
/// Gibt das Kommando ins Spiel. Gerufen wird das erst, wenn feststeht, dass der
/// Absender es ausfuehren darf.
///
/// **Eine Zeile, wo ein Verteiler stand.** Vorher lag unter execute/ je eine Datei
/// pro Kommandoart, und diese Funktion suchte die passende heraus. Jetzt gibt es
/// einen Weg hinein, und das Spiel entscheidet selbst, was zu tun ist: wer etwas
/// will, steckt sein Kommando in RAD_GameExecuteCommand. Damit kennt der Server
/// keine Kommandoart mehr -- er weiss, wer senden darf, und nicht, was das Senden
/// bewirkt.
///
/// **Die Kopie ist noetig und nicht Geschmackssache.** RAD_GameExecuteCommand nimmt
/// das Kommando ohne const, hier liegt es mit -- und die Fassade wird dafuer nicht
/// angefasst. Eine Kopie kostet nichts, was hier ins Gewicht faellt: ein Kommando
/// ist Daten, die sich kopieren lassen, und genau darauf ist es gebaut
/// (control/command/command.h).
///
/// **Es kommt kein Ergebnis zurueck**, denn RAD_GameExecuteCommand gibt void. Der
/// Aufrufer bekommt deshalb RAD_CONTROL_OK, sobald das Kommando uebergeben wurde --
/// nicht, weil es gewirkt haette, sondern weil hier niemand mehr weiss, ob es das
/// tat. Frueher kam die Auskunft von den Ausfuehrenden unter execute/, die in der
/// Welt nachsahen; heute steht die Welt hinter dem Spiel.
///
/// Der Weg dafuer sind die Ereignisse: RAD_OnUnitMoved_t traegt ein "result" und
/// den tatsaechlich gelaufenen Pfad (control/events/event_manager.h), und
/// RAD_EventManagerSubscribeToUnitEvents ist oeffentlich. Wer das Ergebnis in die
/// Antwort holen will, abonniert dort und haelt fest, was zu dem gerade uebergebenen
/// Kommando gemeldet wurde. Solange das nicht steht, ist "value" in der Antwort
/// nicht mehr als "angenommen".
///
static uint32_t RAD_ControlExecuteAllowedCommand(RAD_Control_t control, const RAD_Command_t *command)
{
    RAD_Command_t executable = *command;

    RAD_GameExecuteCommand(control->game, &executable);

    return (uint32_t)RAD_CONTROL_OK;
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

    switch(command->header.type)
    {
        // Der Besitz und dazu, ob die Einheit in diesem Zug noch ziehen darf.
        case RAD_COMMAND_TYPE_MOVE_UNIT:
            return RAD_ControlCheckMove(control, command->header.user, &command->command.move_unit);

        case RAD_COMMAND_TYPE_REMOVE_UNIT:
            return RAD_ControlCheckUnitOwner(control, command->header.user, command->command.remove_unit.unit);

        // Nicht das Ziel wird geprueft, sondern der, der handelt: angegriffen
        // wird ein Feld, und was dort steht, gehoert gerade nicht dem Absender --
        // sonst haette ein Angriff wenig Sinn. Dazu, ob die Einheit in diesem Zug
        // noch angreifen darf.
        case RAD_COMMAND_TYPE_ATTACK:
            return RAD_ControlCheckAttack(control, command->header.user, &command->command.attack);

        // Die ganze Regel des Aufstellens und nicht nur der Besitz: der Absender
        // soll erfahren, warum es nicht geht -- und das Spiel meldet es nicht
        // zurueck (RAD_ControlExecuteAllowedCommand). Ein Spieler bringt seine
        // Einheiten mit der Armee mit; einen Weg, eine Figur an ihr vorbei zu
        // setzen, gibt es nicht.
        case RAD_COMMAND_TYPE_DEPLOY_UNIT:
            return RAD_ControlCheckDeploy(control, command->header.user, &command->command.deploy_unit);

        // Kein Besitz zu pruefen: diese Kommandos fassen keine vorhandene Figur
        // an. Wer Gelaende legen darf, ist eine eigene Frage und noch offen --
        // heute darf es jeder, der dran ist. Und wer abgibt, fasst gar nichts an.
        case RAD_COMMAND_TYPE_CREATE_TILE:
        case RAD_COMMAND_TYPE_REMOVE_TILE:
        case RAD_COMMAND_TYPE_END_TURN:
            break;

        // Benutzt wird ein Feld wie beim Angriff: geprueft wird, wer benutzt.
        case RAD_COMMAND_TYPE_USE:
            return RAD_ControlCheckUnitOwner(control, command->header.user, command->command.use.unit);

        // Unerreichbar: aus dem Codec kommt kein Kommando ohne Art.
        case RAD_COMMAND_TYPE_NONE:
        default:
            break;
    }

    return RAD_CONTROL_OK;
}

///
///
/// Darf dieser Mitspieler diese Figur anfassen? Die Regel dazu steht im Spiel
/// (RAD_GameMayControlUnit), hier wird sie nur auf eine Antwort abgebildet, die
/// ueber die Strecke geht.
///
static RAD_ControlResult_t RAD_ControlCheckUnitOwner(RAD_Control_t control, RAD_UserId_t user, RAD_UnitId_t unit)
{
    if(!RAD_GameMayControlUnit(control->game, user, unit))
    {
        return RAD_CONTROL_ERROR_NOT_OWNED;
    }

    return RAD_CONTROL_OK;
}

///
/// Die Regel des Aufstellens (RAD_GameCheckDeployUnit), abgebildet auf Antworten,
/// die ueber die Strecke gehen. Mitspielen hat RAD_ControlCheckCommand schon
/// geprueft; der Fall steht hier trotzdem, damit kein Ergebnis des Spiels ohne
/// Antwort bleibt.
///
static RAD_ControlResult_t RAD_ControlCheckDeploy(RAD_Control_t control, RAD_UserId_t user, const RAD_CommandDeployUnit_t *deploy)
{
    switch(RAD_GameCheckDeployUnit(control->game, user, deploy->unit, deploy->x, deploy->y))
    {
        case RAD_GAME_OK:                    return RAD_CONTROL_OK;
        case RAD_GAME_ERROR_NO_USER:         return RAD_CONTROL_ERROR_NO_USER;
        case RAD_GAME_ERROR_NOT_PLAYING:     return RAD_CONTROL_ERROR_NOT_PLAYING;
        case RAD_GAME_ERROR_NO_UNIT:         return RAD_CONTROL_ERROR_NO_SUCH_UNIT;
        case RAD_GAME_ERROR_NOT_OWNED:       return RAD_CONTROL_ERROR_NOT_OWNED;
        case RAD_GAME_ERROR_NOT_IN_RESERVE:  return RAD_CONTROL_ERROR_NOT_IN_RESERVE;
        case RAD_GAME_ERROR_OUT_OF_BOUNDS:   return RAD_CONTROL_ERROR_OUT_OF_BOUNDS;
        case RAD_GAME_ERROR_NO_GROUND:       return RAD_CONTROL_ERROR_NO_GROUND;
        case RAD_GAME_ERROR_OCCUPIED:        return RAD_CONTROL_ERROR_TARGET_OCCUPIED;
        default:                             return RAD_CONTROL_ERROR_NOT_EXECUTED;
    }
}

///
/// Ziehen: erst der Besitz, dann ob die Einheit in diesem Zug noch ziehen darf
/// (RAD_GameCheckMoveUnit).
///
static RAD_ControlResult_t RAD_ControlCheckMove(RAD_Control_t control, RAD_UserId_t user, const RAD_CommandMoveUnit_t *move)
{
    const RAD_ControlResult_t owner = RAD_ControlCheckUnitOwner(control, user, move->unit);
    if(owner != RAD_CONTROL_OK)
    {
        return owner;
    }

    return RAD_ControlFromUnitActionResult(RAD_GameCheckMoveUnit(control->game, move->unit));
}

///
/// Angreifen: erst der Besitz, dann ob die Einheit in diesem Zug noch angreifen
/// darf (RAD_GameCheckAttack).
///
static RAD_ControlResult_t RAD_ControlCheckAttack(RAD_Control_t control, RAD_UserId_t user, const RAD_CommandAttack_t *attack)
{
    const RAD_ControlResult_t owner = RAD_ControlCheckUnitOwner(control, user, attack->unit);
    if(owner != RAD_CONTROL_OK)
    {
        return owner;
    }

    return RAD_ControlFromUnitActionResult(RAD_GameCheckAttack(control->game, attack->unit, attack->x, attack->y));
}

///
/// Die Ergebnisse von RAD_GameCheckMoveUnit und RAD_GameCheckAttack, abgebildet
/// auf Antworten, die ueber die Strecke gehen.
///
static RAD_ControlResult_t RAD_ControlFromUnitActionResult(RAD_GameResult_t result)
{
    switch(result)
    {
        case RAD_GAME_OK:                           return RAD_CONTROL_OK;
        case RAD_GAME_ERROR_NO_UNIT:                return RAD_CONTROL_ERROR_NO_SUCH_UNIT;
        case RAD_GAME_ERROR_UNIT_NOT_DEPLOYED:      return RAD_CONTROL_ERROR_UNIT_NOT_DEPLOYED;
        case RAD_GAME_ERROR_UNIT_JUST_DEPLOYED:     return RAD_CONTROL_ERROR_UNIT_JUST_DEPLOYED;
        case RAD_GAME_ERROR_UNIT_ALREADY_MOVED:     return RAD_CONTROL_ERROR_UNIT_ALREADY_MOVED;
        case RAD_GAME_ERROR_UNIT_ALREADY_ATTACKED:  return RAD_CONTROL_ERROR_UNIT_ALREADY_ATTACKED;
        case RAD_GAME_ERROR_OUT_OF_BOUNDS:          return RAD_CONTROL_ERROR_OUT_OF_BOUNDS;
        case RAD_GAME_ERROR_TARGET_OUT_OF_RANGE:    return RAD_CONTROL_ERROR_TARGET_OUT_OF_RANGE;
        default:                                    return RAD_CONTROL_ERROR_NOT_EXECUTED;
    }
}
