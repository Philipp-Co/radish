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
/// schrieb; dazu fuehrte diese Datei den Vorrat des Zuges und eine Preisliste. Beides
/// ist weg: der Weg hinein ist RAD_GameExecuteCommand, und was ein Kommando bewirkt,
/// was es kostet und wann ein Zug vorbei ist, wertet das Spiel selbst aus.
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
static RAD_ControlResult_t RAD_ControlCheckEntityOwner(RAD_Control_t control, RAD_UserId_t user, RAD_EntityId_t entity);
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
/// Der Weg dafuer sind die Ereignisse: RAD_OnEntityMoved_t traegt ein "result" und
/// den tatsaechlich gelaufenen Pfad (control/events/event_manager.h), und
/// RAD_EventManagerSubscribeToEntityEvents ist oeffentlich. Wer das Ergebnis in die
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

    // Was ein Kommando kostet und ob der Absender es sich leisten kann, steht
    // nicht mehr hier: das wertet das Spiel aus, wenn es das Kommando bekommt.
    // Diese Datei kannte dafuer den Vorrat des Zuges und die Preisliste -- beides
    // sind Regeln, und Regeln stehen im Spiel.
    //
    // RAD_CONTROL_ERROR_NOT_ENOUGH_ACTION_POINTS bleibt in der Aufzaehlung stehen,
    // hat aber vorlaeufig keinen Absender mehr: solange RAD_GameExecuteCommand
    // nichts zurueckgibt, kommt aus dem Spiel kein Grund heraus. Der Weg dafuer sind
    // die Ereignisse, siehe RAD_ControlExecuteAllowedCommand.

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
