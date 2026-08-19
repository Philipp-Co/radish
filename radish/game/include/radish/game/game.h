#ifndef __RAD_GAME_H__
#define __RAD_GAME_H__

#include <stdbool.h>
#include <radish/game/model/model.h>
#include <radish/game/user.h>
#include <radish/game/control/events/event_manager.h>
#include <radish/game/control/command/command.h>

///
/// Das Spielmodul zerfaellt in zwei Haelften, und diese Datei ist die Naht.
///
///     model/     der Spielzustand und seine Regeln: Welt, Tiles, Entitaeten,
///                der Zug. Kennt weder Absender noch Abonnenten, nur sich selbst.
///                Ueberwiegend privat, hinter dem Suchpfad von radish_game --
///                offen liegen nur die zwei Strukturen, die aus dem Spiel
///                herauskommen (model/tile/tile.h, model/entity/entity.h); Welt,
///                Zug und Spiel bleiben Namen. Die Begruendung steht in model.h.
///
///     control/   was von aussen daran geschieht: command/ der Weg hinein,
///                events/ der Weg hinaus, execute/ die Fabriken dazwischen.
///
/// **RAD_Game_t ist hier nur ein Name** (model.h); die Struktur dahinter steht in
/// model/game.h und ist von aussen nicht zu sehen. Ein Aufrufer haelt einen
/// RAD_Game_t* und kommt an den Zustand ueber die Funktionen unten -- nicht ueber
/// Felder. Das ist der Sinn der Trennung: die Regeln lassen sich nicht umgehen,
/// wenn niemand an ihnen vorbei schreiben kann.
///
/// Ein Spiel legt man mit RAD_CreateGame an; der Server reicht Kommandos hinein,
/// der Client haelt eines und zeichnet, was er ueber die Ereignisse erfaehrt.
///

///
/// Legt ein Spiel an und gibt es frei. Der einzige Weg an ein RAD_Game_t zu
/// kommen: die Struktur ist von aussen unvollstaendig, ein Aufrufer kann sie
/// weder auf den Stapel legen noch ihre Groesse erfragen.
///
/// "event_manager" wird nur hinterlegt, nicht uebernommen -- er muss laenger
/// leben als das Spiel und wird nach ihm abgebaut.
///
RAD_Game_t* RAD_CreateGame(RAD_EventManager_t *event_manager, RAD_UserId_t local_user);
void RAD_DestroyGame(RAD_Game_t **game);

///
/// Ergebnis einer Aenderung an Mitspielern, Zug oder Besitz.
///
/// Eigene Aufzaehlung, obwohl der Server eine aehnliche fuehrt
/// (RAD_ControlResult_t): dessen Werte gehen als "value" ueber die Strecke und
/// sind an das Protokoll gebunden, diese hier an die Regeln. Der Server bildet
/// die einen auf die anderen ab, so wie er es vorher mit seiner eigenen
/// Teilnehmerliste tat.
///
typedef enum
{
    RAD_GAME_OK = 0,

    /// RAD_USER_NONE ist kein Benutzer.
    RAD_GAME_ERROR_NO_USER,

    /// RAD_ENTITY_NONE ist keine Figur, oder sie steht nicht in der Welt.
    RAD_GAME_ERROR_NO_ENTITY,

    /// Kein Platz mehr frei -- RAD_MAX_PLAYERS.
    RAD_GAME_ERROR_FULL,

    /// Der Benutzer spielt nicht mit.
    RAD_GAME_ERROR_NOT_PLAYING,

    /// Die Figur gehoert einem anderen Benutzer.
    RAD_GAME_ERROR_NOT_OWNED,

    /// Ein anderer ist dran.
    RAD_GAME_ERROR_NOT_YOUR_TURN
} RAD_GameResult_t;

///
/// Macht daraus einen Text zum Loggen, wie RAD_CommandCodecResultText. Immer ein
/// gueltiger Zeiger, auch bei einem Wert ausserhalb der Aufzaehlung.
///
const char* RAD_GameResultText(RAD_GameResult_t result);

///
/// Nimmt einen Benutzer auf. Er hat danach noch keine Figur.
///
/// **Der Weg hinein, und der einzige.** Mitspielen heisst, in der Reihe des
/// Zuges zu stehen (turn.h) -- dahinter steht deshalb nur ein Aufruf. Ueber
/// diese Funktion und nicht ueber RAD_TurnAddUser, damit sich aendern kann, was
/// "mitspielen" heisst, ohne dass jede Aufrufstelle davon erfaehrt.
///
/// Angehaengt wird hinten, damit ein Beitritt den laufenden Zug nicht verschiebt:
/// wer dazukommt, ist in dieser Runde noch dran, wenn er hinter dem steht, der
/// gerade zieht, und sonst ab der naechsten. Der erste Beitritt eroeffnet
/// zugleich den ersten Zug, und zwar seinen -- sobald jemand mitspielt, ist auch
/// jemand dran.
///
/// Zweimal derselbe ist RAD_GAME_OK und aendert nichts: der Aufrufer wollte, dass
/// der Benutzer mitspielt, und das tut er. Wer wissen will, ob jemand schon
/// mitspielt, fragt mit RAD_GameIsPlaying -- das ist die Frage, das hier ist die
/// Aenderung.
///
RAD_GameResult_t RAD_GameAddPlayer(RAD_Game_t *game, RAD_UserId_t user);

///
/// Nimmt einen Benutzer heraus; ein unbekannter ist kein Fehler, Gehen ist
/// idempotent.
///
/// **Seine Figuren bleiben stehen und bleiben seine.** Der Besitz haengt an der
/// Uuid und nicht an der Verbindung -- kommt er wieder, fuehrt er sie weiter. Wer
/// sie freigeben will, ruft RAD_GameUnbindEntity; wer sie aus der Welt nehmen
/// will, liest sie vorher aus (RAD_GameNumberOfUserEntities und
/// RAD_GameUserEntityAt, beide in entity.h).
///
/// War er dran, geht der Zug an den naechsten Mitspieler. Sonst wartete die Runde
/// auf jemanden, der nicht mehr da ist.
///
void RAD_GameRemovePlayer(RAD_Game_t *game, RAD_UserId_t user);

bool RAD_GameIsPlaying(const RAD_Game_t *game, RAD_UserId_t user);
int32_t RAD_GameNumberOfPlayers(const RAD_Game_t *game);

///
/// Wer dran ist; RAD_USER_NONE, solange niemand mitspielt.
///
/// Beides geht an den Zug (turn.h). Wer mehr wissen will -- die ganze
/// Reihenfolge, den Vorrat an Aktionspunkten --, fragt ihn ueber "game->turn"
/// direkt; nach aussen gereicht wird hier nur, was auch der Server braucht.
///
RAD_UserId_t RAD_GameCurrentUser(const RAD_Game_t *game);
bool RAD_GameIsUsersTurn(const RAD_Game_t *game, RAD_UserId_t user);

///
/// Beendet den Zug und gibt ihn an den naechsten in der Reihe weiter; dessen
/// Aktionspunkte fangen von vorne an.
///
/// Mit dem Benutzer als Argument, obwohl das Spiel schon weiss, wer dran ist: nur
/// so laesst sich ein Kommando abweisen, das jemand schickt, der nicht an der
/// Reihe ist (RAD_GAME_ERROR_NOT_YOUR_TURN). Ein Aufruf ohne diese Angabe waere
/// die Aufforderung, blind weiterzuschalten.
///
/// Ist nur einer da, ist danach wieder er dran -- mit vollem Vorrat, es ist ja
/// ein neuer Zug.
///
RAD_GameResult_t RAD_GameEndTurn(RAD_Game_t *game, RAD_UserId_t user);

///
/// Wem die Figur gehoert; RAD_USER_NONE, wenn niemandem -- und das ist kein
/// Fehler, sondern der Normalfall fuer alles, was nicht zugeordnet wurde. Eine
/// Figur, die es nicht gibt, gehoert genauso niemandem.
///
RAD_UserId_t RAD_GameEntityOwner(const RAD_Game_t *game, RAD_EntityId_t entity);

///
/// Ordnet einem Benutzer eine Figur zu. Er darf beliebig viele fuehren -- jede
/// weitere kommt hinzu, keine ersetzt eine andere.
///
/// Dieselbe Figur zweimal an denselben Benutzer ist RAD_GAME_OK und aendert
/// nichts. RAD_GAME_ERROR_NOT_OWNED, wenn sie schon einem anderen gehoert: eine
/// Figur hat hoechstens einen Besitzer, sonst waere nicht entscheidbar, wer sie
/// bewegen darf. Sie einem anderen wegzunehmen geht deshalb nur ueber
/// RAD_GameUnbindEntity.
///
RAD_GameResult_t RAD_GameBindEntity(RAD_Game_t *game, RAD_UserId_t user, RAD_EntityId_t entity);

///
/// Loest die Zuordnung; danach gehoert die Figur niemandem.
///
/// Ohne Benutzer, anders als beim Zuordnen: wem sie gehoert, weiss die Figur
/// schon. War sie herrenlos, aendert sich nichts -- der Aufruf ist idempotent und
/// meldet deshalb auch nichts zurueck.
///
void RAD_GameUnbindEntity(RAD_Game_t *game, RAD_EntityId_t entity);

///
/// Darf dieser Benutzer diese Figur anfassen?
///
/// **Herrenlos ist nicht fremd:** eine Figur, die niemandem gehoert, laesst diese
/// Frage durch. Sonst waere heute jede Bewegung abgelehnt -- zugeordnet wird eine
/// Figur ueber RAD_GameBindEntity, und das ruft noch niemand. Sobald das Setzen
/// einer Figur sie ihrem Benutzer anhaengt, greift die Pruefung von selbst.
///
/// Nur der Besitz, sonst nichts: ob der Benutzer mitspielt, ob er dran ist und ob
/// es die Figur ueberhaupt gibt, sind eigene Fragen mit eigenen Antworten.
///
bool RAD_GameMayControlEntity(const RAD_Game_t *game, RAD_UserId_t user, RAD_EntityId_t entity);

///
/// **Den Zustand lesen: das steht bei den Typen, nicht hier.**
///
///     tile.h      RAD_GameNumberOfTiles, RAD_GameTileAt
///     entity.h    RAD_GameNumberOfEntities, RAD_GameEntityAt,
///                 RAD_GameNumberOfUserEntities, RAD_GameUserEntityAt
///
/// Sie nehmen alle ein RAD_Game_t und heissen deshalb RAD_Game*, gehoeren aber zu
/// ihrem Typ: wer Tiles lesen will, soll eine Datei dafuer brauchen und nicht die
/// ganze Fassade, und diese Datei soll nicht mit dem Zubehoer jedes einzelnen Typs
/// wachsen. Beide Header kommen ueber event_manager.h ohnehin mit -- wer game.h
/// einbindet, hat sie also, ohne sie zu nennen.
///
/// Was hier bleibt, ist, was kein einzelner Typ beantwortet: Mitspieler, Zug,
/// Besitz und die Kommandos.
///

///
/// Die drei Fabriken fuellen ein Kommando aus und fuehren nichts aus: Art,
/// Sequenznummer und Absender kommen aus dem Spiel, alles andere aus den
/// Argumenten.
///
/// RAD_GameMoveEntity nimmt den Weg als Ganzes (model/path/path.h) -- wo er
/// anfaengt, sagt die Figur und nicht der Aufrufer. Sie liefert false, wenn der
/// Pfad keiner ist: kein Zeiger, keine Schritte oder mehr als RAD_PATH_MAX_STEPS.
/// Dann bleibt "output" unberuehrt und die Sequenznummer stehen.
///
bool RAD_GameSpawnEntity(RAD_Game_t *game, RAD_EntityType_t type, int32_t x, int32_t y, int32_t z, RAD_Command_t *output);
bool RAD_GameDestroyEntity(RAD_Game_t *game, RAD_EntityId_t id, RAD_Command_t *output);
bool RAD_GameMoveEntity(RAD_Game_t *game, RAD_EntityId_t id, const RAD_EntityPath_t *path, RAD_Command_t *output);

void RAD_GameExecuteCommand(RAD_Game_t *game, RAD_Command_t *command);
void RAD_GameRollbackLastCommand(RAD_Game_t *game);

#endif
