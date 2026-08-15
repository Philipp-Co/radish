#ifndef __RAD_CONTROL_EXECUTE_H__
#define __RAD_CONTROL_EXECUTE_H__

#include <stdint.h>
#include <radish/game/game.h>
#include <radish/game/entity.h>
#include <radish/game/user.h>
#include <radish/game/control/command/command.h>

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
/// (radish/game/turn.h), und wo sie steht, die Welt.
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
    /// Ausgefuehrt -- und bei RAD_ControlAddUser/RAD_ControlBindUserEntity:
    /// erledigt. Als Antwort auf ein Kommando kommt der Wert bisher nur von
    /// move_entity; die anderen Arten liefern RAD_CONTROL_ERROR_NOT_EXECUTED.
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

    /// Aufruf ohne Figur: RAD_ENTITY_NONE ist keine.
    RAD_CONTROL_ERROR_NO_ENTITY,

    ///
    /// Ab hier: das Kommando durfte ausgefuehrt werden, aber das Spiel liess es
    /// nicht zu. Der Zustand bleibt in allen drei Faellen unveraendert.
    ///

    /// Die Figur steht nicht in der Welt.
    RAD_CONTROL_ERROR_NO_SUCH_ENTITY,

    /// Das Zielfeld liegt ausserhalb der Welt.
    RAD_CONTROL_ERROR_OUT_OF_BOUNDS,

    /// Auf dem Zielfeld steht schon eine Figur -- pro Tile hoechstens eine.
    RAD_CONTROL_ERROR_TARGET_OCCUPIED,

    ///
    /// Die beiden letzten gehoeren der Sache nach zur ersten Gruppe -- sie sagen,
    /// dass der Absender nicht durfte, nicht dass das Spiel nicht konnte. Sie
    /// stehen trotzdem hier: die Werte gehen ueber die Strecke, und ein Einschub
    /// in der Mitte wuerde alles dahinter umdeuten.
    ///

    /// Ein anderer ist an der Reihe.
    RAD_CONTROL_ERROR_NOT_YOUR_TURN,

    ///
    /// Der Absender ist dran, aber das Kommando kostet mehr Aktionspunkte, als er
    /// noch hat.
    ///
    RAD_CONTROL_ERROR_NOT_ENOUGH_ACTION_POINTS
} RAD_ControlResult_t;

///
/// Macht daraus einen Text zum Loggen, wie RAD_CommandCodecResultText. Immer ein
/// gueltiger Zeiger, auch bei einem Wert ausserhalb der Aufzaehlung.
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
/// Nimmt einen Benutzer wieder heraus. Ein unbekannter ist kein Fehler.
///
/// Seine Figuren bleiben in der Welt stehen und bleiben seine: der Besitz haengt
/// an der Uuid und nicht an der Verbindung, kommt er wieder, fuehrt er sie
/// weiter. Wer sie freigeben oder aus der Welt nehmen will, liest sie vorher aus
/// (RAD_ControlNumberOfUserEntities und RAD_ControlUserEntityAt).
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
/// sie bewegen darf. RAD_CONTROL_ERROR_NO_ENTITY fuer RAD_ENTITY_NONE und fuer
/// jede Id, hinter der keine Figur in der Welt steht: der Besitz haengt seit
/// neuestem an der Figur selbst, eine Zuordnung ins Leere gibt es damit nicht
/// mehr.
///
RAD_ControlResult_t RAD_ControlBindUserEntity(RAD_Control_t control, RAD_UserId_t user, RAD_EntityId_t entity);

///
/// Loest die Zuordnung einer Figur; danach gehoert sie niemandem.
///
/// Ohne Benutzer, anders als beim Zuordnen: wem sie gehoert, weiss die Figur
/// schon. War sie herrenlos, aendert sich nichts -- wie beim Herausnehmen eines
/// Benutzers ist der Aufruf idempotent und meldet deshalb auch nichts zurueck.
///
void RAD_ControlUnbindEntity(RAD_Control_t control, RAD_EntityId_t entity);

///
/// Die Figuren eines Benutzers: erst zaehlen, dann einzeln holen. Ohne die beiden
/// waere die Zuordnung von aussen nicht nachzulesen -- etwa um beim Verlassen die
/// Figuren aus der Welt zu nehmen.
///
/// RAD_ControlUserEntityAt liefert RAD_ENTITY_NONE fuer einen Index ausserhalb
/// [0, RAD_ControlNumberOfUserEntities). Gezaehlt wird in der Reihenfolge der
/// Ids, ein Index gilt also, solange dem Benutzer keine Figur dazukommt oder
/// wegfaellt.
///
int32_t RAD_ControlNumberOfUserEntities(RAD_Control_t control, RAD_UserId_t user);
RAD_EntityId_t RAD_ControlUserEntityAt(RAD_Control_t control, RAD_UserId_t user, int32_t index);

///
/// Die Gegenrichtung: wem gehoert diese Figur? RAD_USER_NONE, wenn niemandem --
/// und das ist kein Fehler, sondern der Normalfall fuer alles, was nicht gesetzt
/// wurde.
///
RAD_UserId_t RAD_ControlEntityOwner(RAD_Control_t control, RAD_EntityId_t entity);

/// Wie viele mitspielen -- fuers Log.
int32_t RAD_ControlNumberOfPlayers(RAD_Control_t control);

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
/// das Erste fest, uebernimmt je eine Datei unter control/execute/ die Ausfuehrung
/// ihrer Kommandoart -- bisher move_entity und end_turn, der Rest liefert
/// RAD_CONTROL_ERROR_NOT_EXECUTED. Die Ausfuehrenden fragen nicht mehr nach
/// Besitz oder Zug: dass der Benutzer darf, ist entschieden, bevor sie gerufen
/// werden.
///
/// Geprueft wird dreierlei: der Absender muss mitspielen, an der Reihe sein
/// (RAD_CONTROL_ERROR_NOT_YOUR_TURN) und sich das Kommando leisten koennen
/// (RAD_CONTROL_ERROR_NOT_ENOUGH_ACTION_POINTS); fasst es eine vorhandene Figur
/// an, muss sie ihm gehoeren.
///
/// **Bezahlt wird nach dem Ausfuehren, und nur, was geschehen ist.** Jedes
/// ausgefuehrte Kommando kostet einen Aktionspunkt, das Abgeben des Zuges keinen;
/// was abgelehnt wurde oder noch keinen Ausfuehrenden hat, kostet nichts -- ein
/// Kommando, das den Zustand nicht angefasst hat, darf auch keinen Zug
/// verbrauchen.
///
/// **Und wer nichts mehr kann, ist fertig:** faellt der Vorrat dabei auf null,
/// geht der Zug von selbst an den naechsten. Ein Spieler muss seinen Zug also
/// nicht abgeben, er kann es nur -- mit RAD_COMMAND_TYPE_END_TURN, wenn er etwas
/// uebrig behaelt.
///
/// Der Absender erfaehrt davon nichts weiter: die Antwort traegt das Ergebnis
/// seines Kommandos, nicht den neuen Zustand des Zuges. Wie ein Client mitbekommt,
/// dass er dran ist, ist eine Frage des Protokolls und noch offen.
///
RAD_CommandResponse_t RAD_ControlExecuteCommand(RAD_Control_t control, const RAD_Command_t *command);

#endif
