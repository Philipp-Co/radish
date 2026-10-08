#ifndef __RAD_CONTROL_EXECUTE_H__
#define __RAD_CONTROL_EXECUTE_H__

#include <stdint.h>
#include <radish/game/game.h>
#include <radish/game/user.h>
#include <radish/game/control/command/command.h>
#include <radish/game/model/tile/tile.h>
#include <radish/game/control/start_game.h>

///
/// control/ -- was mit einem Kommando geschieht. Es entscheidet, ob das Kommando
/// gilt, fuehrt es aus und beantwortet es.
///
///     Nachricht ──►│ interface/ │──► RAD_Command_t ──►│ control/ │──► Spiel
///     Nachricht ◄──│            │◄── RAD_CommandResponse_t ◄──────┘
///
/// Die Grenze zu interface/ ist scharf: dort geht es um Bytes, hier um Bedeutung.
/// interface/ liest ein Kommando, ohne zu wissen, was es anrichtet; dieses Modul
/// sieht nie eine Nachricht und weiss nicht, wo sie herkam.
///
/// Es ist die einzige Stelle im Server, die entscheidet, ob ein Kommando gilt.
/// Wissen muss es dafuer nichts selbst: wer es geschickt hat, steht im Kopf
/// (RAD_UserId_t), wer mitspielt und wem die Figur gehoert, weiss das Spiel
/// (radish/game/model/turn/turn.h), und wo sie steht, die Welt.
///
/// **Die Teilnehmerliste gehoert dem Spiel, nicht diesem Modul.** Sie lag einmal
/// hier -- als src/control/session/ neben dieser Uebersetzungseinheit --, solange
/// nur der Server sie brauchte. Seit das Spiel auch weiss, wer dran ist, waere
/// das zwei Buecher: der Zug laeuft ueber die Mitspieler, und ein Client haelt ein
/// Spiel, aber keine Steuerung. Die Funktionen unten sind seitdem
/// Weiterleitungen.
///
/// Der Weg nach draussen bleiben sie trotzdem: wer einen Benutzer aufnimmt, tut
/// das ueber sie, damit "ob ein Kommando gilt" an einer Stelle entschieden wird.
///

///
/// Der Zustand, den control/ fuehrt: das Spiel, auf das sich alles bezieht.
///
/// Unvollstaendiger Typ wie ZUC_Api_t -- was drinsteht, weiss nur execute.c. Das
/// bleibt so, obwohl es derzeit nur ein Zeiger ist: was der Server sich neben dem
/// Spiel merken muss (Verbindungen, offene Antworten), kommt hier hinein, ohne
/// dass ein Aufrufer davon etwas mitbekommt.
///
struct RAD_Control;
typedef struct RAD_Control* RAD_Control_t;

///
/// Ergebnis des Ausfuehrens. Es geht als "value" in die Antwort und damit ueber
/// die Strecke -- neue Werte gehoeren deshalb ans Ende, sonst deuten sie
/// unterwegs die dahinter um. Dasselbe Motiv wie bei den Wire-Nummern des Codecs.
///
typedef enum
{
    ///
    /// Ausgefuehrt -- und bei RAD_ControlAddUser/RAD_ControlBindUserUnit:
    /// erledigt. Als Antwort auf ein Kommando kommt der Wert bisher nur von
    /// move_unit; die anderen Arten liefern RAD_CONTROL_ERROR_NOT_EXECUTED.
    ///
    RAD_CONTROL_OK = 0,

    /// Kommando ohne Absender, RAD_USER_NONE im Kopf.
    RAD_CONTROL_ERROR_NO_USER,

    /// Der Absender spielt nicht mit und konnte auch nicht aufgenommen werden.
    RAD_CONTROL_ERROR_NOT_PLAYING,

    /// Die Figur, die das Kommando anfasst, gehoert einem anderen Benutzer.
    RAD_CONTROL_ERROR_NOT_OWNED,

    ///
    /// Nichts sprach dagegen, aber die Art wird noch nicht ausgefuehrt. Der
    /// Spielzustand bleibt unberuehrt.
    ///
    RAD_CONTROL_ERROR_NOT_EXECUTED,

    /// Aufruf ohne Figur: RAD_UNIT_NONE ist keine.
    RAD_CONTROL_ERROR_NO_UNIT,

    ///
    /// Ab hier: das Kommando durfte ausgefuehrt werden, aber das Spiel liess es
    /// nicht zu. Der Zustand bleibt in allen drei Faellen unveraendert.
    ///

    /// Die Figur steht nicht in der Welt.
    RAD_CONTROL_ERROR_NO_SUCH_UNIT,

    /// Das Zielfeld liegt ausserhalb der Welt.
    RAD_CONTROL_ERROR_OUT_OF_BOUNDS,

    /// Auf dem Zielfeld steht schon eine Figur -- pro Tile hoechstens eine.
    RAD_CONTROL_ERROR_TARGET_OCCUPIED,

    ///
    /// Der letzte gehoert der Sache nach zur ersten Gruppe -- er sagt, dass der
    /// Absender nicht durfte, nicht dass das Spiel nicht konnte. Er steht
    /// trotzdem hier: neue Werte kommen hinten an, statt die dahinter umzudeuten.
    ///

    /// Ein anderer ist an der Reihe.
    RAD_CONTROL_ERROR_NOT_YOUR_TURN,

    ///
    /// Aus dem Aufstellen (RAD_COMMAND_TYPE_DEPLOY_UNIT), hinten angehaengt aus
    /// demselben Grund wie der davor. Die uebrigen Gruende dafuer haben
    /// schon einen Wert: NOT_OWNED, NO_SUCH_UNIT, OUT_OF_BOUNDS, TARGET_OCCUPIED.
    ///

    /// Die Einheit steht nicht in der Reserve -- schon auf dem Feld, oder zerstoert.
    RAD_CONTROL_ERROR_NOT_IN_RESERVE,

    /// Das Zielfeld hat kein Gelaende.
    RAD_CONTROL_ERROR_NO_GROUND,

    ///
    /// Aus Ziehen und Angreifen (RAD_COMMAND_TYPE_MOVE_UNIT,
    /// RAD_COMMAND_TYPE_ATTACK): was die Einheit in diesem Zug schon getan hat
    /// (RAD_GameCheckMoveUnit, RAD_GameCheckAttack).
    ///

    /// Die Einheit ist in diesem Zug erst aufgestellt worden.
    RAD_CONTROL_ERROR_UNIT_JUST_DEPLOYED,

    /// Die Einheit ist in diesem Zug schon gezogen.
    RAD_CONTROL_ERROR_UNIT_ALREADY_MOVED,

    /// Die Einheit hat in diesem Zug schon angegriffen.
    RAD_CONTROL_ERROR_UNIT_ALREADY_ATTACKED,

    /// Die Einheit steht nicht auf dem Feld -- noch in der Reserve, oder zerstoert.
    RAD_CONTROL_ERROR_UNIT_NOT_DEPLOYED,

    /// Keine Waffe der Einheit reicht bis zum Ziel des Angriffs.
    RAD_CONTROL_ERROR_TARGET_OUT_OF_RANGE
} RAD_ControlResult_t;

///
/// Macht daraus einen Text zum Loggen, wie RAD_NetCodecResultText
/// (interface/message.h). Immer ein gueltiger Zeiger, auch bei einem Wert
/// ausserhalb der Aufzaehlung.
///
const char* RAD_ControlResultText(RAD_ControlResult_t result);

///
/// Legt die Steuerung an. NULL, wenn kein Speicher da ist.
///
/// "game" wird nur hinterlegt, nicht uebernommen: es muss laenger leben als die
/// Steuerung und wird nach ihr abgebaut, wie der Event-Manager beim Spiel.
///
RAD_Control_t RAD_CreateControl(RAD_Game_t *game);
void RAD_DestroyControl(RAD_Control_t *control);

///
/// Nimmt einen Benutzer auf. Danach spielt er mit -- auch dann, wenn er es schon
/// vorher tat: zweimal aufnehmen ist kein Fehler, sondern derselbe Zustand.
///
/// RAD_CONTROL_ERROR_NO_USER fuer RAD_USER_NONE, RAD_CONTROL_ERROR_NOT_PLAYING,
/// wenn kein Platz mehr frei ist -- der Name sagt, was danach gilt.
///
RAD_ControlResult_t RAD_ControlAddUser(RAD_Control_t control, RAD_UserId_t user);

///
/// Richtet das Spiel nach dem Spielstart ein: beide Spieler spielen danach mit,
/// der Host zuerst und damit auch zuerst am Zug, und ihre Armeen stehen in ihrer
/// Reserve -- dem Spiel bekannt, auf dem Feld noch nicht.
///
/// Beides reicht nur durch: wie aus einer Spieldatei ein Spiel wird, steht im
/// Spielmodul (radish/game/control/start_game.h, RAD_StartGame und
/// RAD_StartGameFromFile), samt den Ergebnissen und der Regel "genau einmal".
/// Ein zweites Signal fuer dieselbe Datei richtet damit keinen Schaden an.
///
/// RAD_ControlStartGameFromFile ist der Weg des Servers (main.c, nach SIGUSR1);
/// RAD_ControlStartGame nimmt einen schon gelesenen Start, fuer die Tests.
///
RAD_GameResult_t RAD_ControlStartGame(RAD_Control_t control, const RAD_GameStart_t *start);
RAD_GameStartResult_t RAD_ControlStartGameFromFile(RAD_Control_t control, const char *path, RAD_GameResult_t *game_result);

///
/// Nimmt einen Benutzer wieder heraus. Ein unbekannter ist kein Fehler.
///
/// Seine Figuren bleiben in der Welt stehen und bleiben seine: der Besitz haengt
/// an der Uuid und nicht an der Verbindung, kommt er wieder, fuehrt er sie
/// weiter. Wer sie freigeben oder aus der Welt nehmen will, liest sie vorher aus
/// (RAD_ControlNumberOfUserUnits und RAD_ControlUserUnitAt).
///
/// War er dran, geht der Zug an den naechsten Mitspieler -- das entscheidet das
/// Spiel (RAD_GameRemovePlayer), nicht dieses Modul.
///
void RAD_ControlRemoveUser(RAD_Control_t control, RAD_UserId_t user);

///
/// Ordnet einem Benutzer eine Figur zu. Er darf beliebig viele fuehren -- jede
/// weitere kommt hinzu, keine ersetzt eine andere.
///
/// Erst danach greift die Besitzpruefung beim Ausfuehren: solange eine Figur
/// niemandem gehoert, darf jeder Mitspieler sie anfassen.
///
/// Dieselbe Figur zweimal an denselben Benutzer ist RAD_CONTROL_OK und aendert
/// nichts. RAD_CONTROL_ERROR_NOT_PLAYING, wenn der Benutzer nicht mitspielt;
/// RAD_CONTROL_ERROR_NOT_OWNED, wenn die Figur schon einem anderen gehoert --
/// eine Figur hat hoechstens einen Besitzer, sonst waere nicht entscheidbar, wer
/// sie bewegen darf. RAD_CONTROL_ERROR_NO_UNIT fuer RAD_UNIT_NONE und fuer
/// jede Id, hinter der keine Figur in der Welt steht: der Besitz haengt seit
/// neuestem an der Figur selbst, eine Zuordnung ins Leere gibt es damit nicht
/// mehr.
///
RAD_ControlResult_t RAD_ControlBindUserUnit(RAD_Control_t control, RAD_UserId_t user, RAD_UnitId_t unit);

///
/// Loest die Zuordnung einer Figur; danach gehoert sie niemandem.
///
/// Ohne Benutzer, anders als beim Zuordnen: wem sie gehoert, weiss die Figur
/// schon. War sie herrenlos, aendert sich nichts -- wie beim Herausnehmen eines
/// Benutzers ist der Aufruf idempotent und meldet deshalb auch nichts zurueck.
///
void RAD_ControlUnbindUnit(RAD_Control_t control, RAD_UnitId_t unit);

///
/// Die Figuren eines Benutzers: erst zaehlen, dann einzeln holen. Ohne die beiden
/// waere die Zuordnung von aussen nicht nachzulesen -- etwa um beim Verlassen die
/// Figuren aus der Welt zu nehmen.
///
/// RAD_ControlUserUnitAt liefert RAD_UNIT_NONE fuer einen Index ausserhalb
/// [0, RAD_ControlNumberOfUserUnits). Gezaehlt wird in der Reihenfolge der
/// Ids, ein Index gilt also, solange dem Benutzer keine Figur dazukommt oder
/// wegfaellt.
///
int32_t RAD_ControlNumberOfUserUnits(RAD_Control_t control, RAD_UserId_t user);
RAD_UnitId_t RAD_ControlUserUnitAt(RAD_Control_t control, RAD_UserId_t user, int32_t index);

///
/// Die Gegenrichtung: wem gehoert diese Figur? RAD_USER_NONE, wenn niemandem --
/// und das ist kein Fehler, sondern der Normalfall fuer alles, was nicht gesetzt
/// wurde.
///
RAD_UserId_t RAD_ControlUnitOwner(RAD_Control_t control, RAD_UnitId_t unit);

///
/// Alle Einheiten des Spiels zum Nachlesen, in jedem Zustand -- dieselbe Zaehlung
/// wie RAD_GameNumberOfUnits und RAD_GameUnitAt (unit.h), aufsteigend nach Id.
/// Gebraucht fuer die Antwort auf eine Reserve-Anfrage (main.c), die daraus die
/// Einheiten in der Reserve nimmt.
///
int32_t RAD_ControlNumberOfUnits(RAD_Control_t control);
bool RAD_ControlUnitAt(RAD_Control_t control, int32_t index, RAD_Unit_t *output);

/// Wie viele mitspielen -- fuers Log.
int32_t RAD_ControlNumberOfPlayers(RAD_Control_t control);

///
/// Was ein Client nach einer Discover-Anfrage erfaehrt: wer dran ist, wer in
/// welcher Reihenfolge mitspielt und wie gross die Welt ist. Nur gelesen, und
/// alles aus dem Spiel -- RAD_GameCurrentUser, RAD_GamePlayerAt,
/// RAD_GameWorldWidth und RAD_GameWorldHeight.
///
RAD_UserId_t RAD_ControlCurrentUser(RAD_Control_t control);
RAD_UserId_t RAD_ControlPlayerAt(RAD_Control_t control, int32_t index);
int32_t RAD_ControlWorldWidth(RAD_Control_t control);
int32_t RAD_ControlWorldHeight(RAD_Control_t control);

///
/// Ein Feld der Welt mit seinen Attributen, als Kopie nach "output" --
/// RAD_GameTileAt.
///
/// **Anders als dort ist eine Stelle ausserhalb kein Rechenfehler, sondern ein
/// false.** RAD_GameTileAt prueft per assert, weil eine falsche Stelle dort aus
/// dem eigenen Code kommt. Hier kommt sie von aussen: der Ausschnitt einer
/// Discover-Anfrage ist, was ein Client geschickt hat, und der darf den Server
/// nicht anhalten. Deshalb wird vorher gegen die Groesse der Welt geprueft;
/// "output" bleibt bei false unberuehrt.
///
bool RAD_ControlTileAt(RAD_Control_t control, int32_t x, int32_t y, RAD_Tile_t *output);

///
/// Fuehrt ein Kommando aus und beantwortet es.
///
/// Das Kommando geht const hinein und kommt in der Antwort als genaue Kopie
/// wieder heraus. Es ist der Anlass, nicht der Zustand: was es bewirkt, steht
/// danach im Spiel, nicht im Kommando. Und der Absender muss es
/// unveraendert wiederfinden -- nur daran erkennt er, worauf die Antwort geht.
///
/// Die Antwort entsteht hier und nicht in interface/, weil hier das steht, was in
/// ihr Neues drinsteht: "value" traegt RAD_ControlResult_t. Kopf und Kommando
/// kommen aus derselben Quelle, womit die Invariante header == command.header
/// gilt, auf die sich der Codec verlaesst
/// (RAD_COMMAND_CODEC_ERROR_HEADER_MISMATCH).
///
/// Aufgenommen wird hier niemand: wer nicht mitspielt, bekommt
/// RAD_CONTROL_ERROR_NOT_PLAYING. Ein Beitritt ist eine Entscheidung ueber das
/// Protokoll und gehoert dorthin, wo die Nachrichten ankommen --
/// RAD_ControlAddUser steht dafuer bereit.
///
/// Zwei Schritte, in dieser Reihenfolge: erst darf-er-das, dann geht-das. Steht
/// das Erste fest, geht das Kommando ins Spiel -- RAD_GameExecuteCommand ist der
/// eine Weg hinein, und was zu tun ist, wertet das Spiel selbst aus. Der Server
/// kennt dafuer keine Kommandoart mehr; die Dateien unter control/execute/, die
/// vorher je eine ausfuehrten, gibt es nicht mehr.
///
/// Geprueft wird dreierlei: der Absender muss mitspielen, an der Reihe sein
/// (RAD_CONTROL_ERROR_NOT_YOUR_TURN), und fasst das Kommando eine vorhandene Figur
/// an, muss sie ihm gehoeren (RAD_CONTROL_ERROR_NOT_OWNED). Alles daran ist eine
/// Frage des Protokolls und mit den Lesefunktionen des Spiels zu beantworten.
///
/// Bei Ziehen und Angreifen dazu, ob die Einheit das in diesem Zug noch darf
/// (RAD_GameCheckMoveUnit, RAD_GameCheckAttack): nicht frisch aufgestellt, nicht
/// schon gezogen bzw. angegriffen. Das ist eine Regel des Spiels; sie wird hier
/// nur vorab gefragt, damit der Absender den Grund erfaehrt.
///
/// **"value" sagt sonst nur, dass das Kommando angenommen wurde.**
/// RAD_GameExecuteCommand gibt void zurueck, also kommt aus dem Spiel beim
/// Ausfuehren kein Grund heraus -- etwa "Zielfeld besetzt".
/// RAD_CONTROL_ERROR_NOT_EXECUTED steht deshalb ohne Absender in der Aufzaehlung.
/// Der Weg, sie zurueckzuholen, sind die Ereignisse: RAD_OnUnitMoved_t traegt ein
/// "result" und den tatsaechlich gelaufenen Pfad.
///
/// Wie ein Client mitbekommt, dass er dran ist, ist eine Frage des Protokolls und
/// weiter offen.
///
RAD_CommandResponse_t RAD_ControlExecuteCommand(RAD_Control_t control, const RAD_Command_t *command);

#endif
