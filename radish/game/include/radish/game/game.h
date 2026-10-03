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
///     model/     der Spielzustand und seine Regeln: Welt, Tiles, Einheiten,
///                der Zug. Kennt weder Absender noch Abonnenten, nur sich selbst.
///                Ueberwiegend privat, hinter dem Suchpfad von radish_game --
///                offen liegen nur die zwei Strukturen, die aus dem Spiel
///                herauskommen (model/tile/tile.h, model/unit/unit.h); Welt,
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

    /// RAD_UNIT_NONE ist keine Figur, oder sie steht nicht in der Welt.
    RAD_GAME_ERROR_NO_UNIT,

    /// Kein Platz mehr frei -- RAD_MAX_PLAYERS.
    RAD_GAME_ERROR_FULL,

    /// Der Benutzer spielt nicht mit.
    RAD_GAME_ERROR_NOT_PLAYING,

    /// Die Figur gehoert einem anderen Benutzer.
    RAD_GAME_ERROR_NOT_OWNED,

    /// Ein anderer ist dran.
    RAD_GAME_ERROR_NOT_YOUR_TURN,

    /// Die Werte einer Einheit passen nicht in ihre festen Felder (unit.h).
    RAD_GAME_ERROR_INVALID_UNIT,

    /// Das Spiel laeuft schon: der erste Zug ist beendet, die Armeen stehen fest.
    RAD_GAME_ERROR_STARTED,

    /// Die Einheit steht nicht in der Reserve -- schon auf dem Feld, oder zerstoert.
    RAD_GAME_ERROR_NOT_IN_RESERVE,

    /// Das Feld liegt ausserhalb der Welt.
    RAD_GAME_ERROR_OUT_OF_BOUNDS,

    /// Das Feld hat kein Gelaende (RAD_TILE_TYPE_VOID).
    RAD_GAME_ERROR_NO_GROUND,

    /// Auf dem Feld steht schon eine Einheit.
    RAD_GAME_ERROR_OCCUPIED
} RAD_GameResult_t;

///
/// Macht daraus einen Text zum Loggen, wie RAD_ControlResultText. Immer ein
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
/// sie freigeben will, ruft RAD_GameUnbindUnit; wer sie aus der Welt nehmen
/// will, liest sie vorher aus (RAD_GameNumberOfUserUnits und
/// RAD_GameUserUnitAt, beide in unit.h).
///
/// War er dran, geht der Zug an den naechsten Mitspieler. Sonst wartete die Runde
/// auf jemanden, der nicht mehr da ist.
///
void RAD_GameRemovePlayer(RAD_Game_t *game, RAD_UserId_t user);

bool RAD_GameIsPlaying(const RAD_Game_t *game, RAD_UserId_t user);
int32_t RAD_GameNumberOfPlayers(const RAD_Game_t *game);

///
/// Der Mitspieler an Stelle "index" der Zugreihenfolge, 0 ist der, der die Runde
/// eroeffnet. RAD_USER_NONE fuer einen Index ausserhalb
/// [0, RAD_GameNumberOfPlayers). Mit beiden zusammen laesst sich die Reihe von
/// aussen ablaufen, ohne dass ihre Groesse bekannt sein muss.
///
RAD_UserId_t RAD_GamePlayerAt(const RAD_Game_t *game, int32_t index);

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
/// Der erste beendete Zug schliesst die Aufstellung: danach nimmt das Spiel keine
/// Einheiten mehr in die Reserve auf (RAD_GameAddUnit).
///
RAD_GameResult_t RAD_GameEndTurn(RAD_Game_t *game, RAD_UserId_t user);

///
/// Ob schon ein Zug beendet wurde -- dann ist die Aufstellung vorbei, und das
/// Spiel nimmt keine Einheiten mehr in die Reserve auf (RAD_GameAddUnit). false
/// fuer ein NULL-Spiel.
///
bool RAD_GameHasStarted(const RAD_Game_t *game);

///
/// Wem die Figur gehoert; RAD_USER_NONE, wenn niemandem -- und das ist kein
/// Fehler, sondern der Normalfall fuer alles, was nicht zugeordnet wurde. Eine
/// Figur, die es nicht gibt, gehoert genauso niemandem.
///
RAD_UserId_t RAD_GameUnitOwner(const RAD_Game_t *game, RAD_UnitId_t unit);

///
/// Ordnet einem Benutzer eine Figur zu. Er darf beliebig viele fuehren -- jede
/// weitere kommt hinzu, keine ersetzt eine andere.
///
/// Dieselbe Figur zweimal an denselben Benutzer ist RAD_GAME_OK und aendert
/// nichts. RAD_GAME_ERROR_NOT_OWNED, wenn sie schon einem anderen gehoert: eine
/// Figur hat hoechstens einen Besitzer, sonst waere nicht entscheidbar, wer sie
/// bewegen darf. Sie einem anderen wegzunehmen geht deshalb nur ueber
/// RAD_GameUnbindUnit.
///
RAD_GameResult_t RAD_GameBindUnit(RAD_Game_t *game, RAD_UserId_t user, RAD_UnitId_t unit);

///
/// Loest die Zuordnung; danach gehoert die Figur niemandem.
///
/// Ohne Benutzer, anders als beim Zuordnen: wem sie gehoert, weiss die Figur
/// schon. War sie herrenlos, aendert sich nichts -- der Aufruf ist idempotent und
/// meldet deshalb auch nichts zurueck.
///
void RAD_GameUnbindUnit(RAD_Game_t *game, RAD_UnitId_t unit);

///
/// Darf dieser Benutzer diese Figur anfassen?
///
/// **Herrenlos ist nicht fremd:** eine Figur, die niemandem gehoert, laesst diese
/// Frage durch. Sonst waere heute jede Bewegung abgelehnt -- zugeordnet wird eine
/// Figur ueber RAD_GameBindUnit, und das ruft noch niemand. Sobald das Setzen
/// einer Figur sie ihrem Benutzer anhaengt, greift die Pruefung von selbst.
///
/// Nur der Besitz, sonst nichts: ob der Benutzer mitspielt, ob er dran ist und ob
/// es die Figur ueberhaupt gibt, sind eigene Fragen mit eigenen Antworten.
///
bool RAD_GameMayControlUnit(const RAD_Game_t *game, RAD_UserId_t user, RAD_UnitId_t unit);

///
/// Nimmt eine Einheit aus der Armee eines Spielers in die Reserve auf: dem Spiel
/// bekannt, auf dem Feld noch nicht. Wie RAD_WorldAddReserveUnit uebernimmt sie
/// aus "values" nur die Werte der Einheit; Id, Zustand und Position vergibt die
/// Welt, Besitzer ist "owner". "id" bekommt die neue Id und darf NULL sein; bei
/// einem Fehler steht dort RAD_UNIT_NONE.
///
/// **Die Regel steht hier, der Pool in der Welt.** Eine Einheit bekommt nur, wer
/// mitspielt, und nur solange das Spiel in der Aufstellung ist -- nach dem ersten
/// beendeten Zug steht fest, wer womit spielt (RAD_GameEndTurn). Das sind Fragen
/// des Spiels, deshalb geht der Weg hierher und nicht direkt an die Welt.
///
///     RAD_GAME_ERROR_NO_USER       "owner" ist RAD_USER_NONE
///     RAD_GAME_ERROR_NOT_PLAYING   "owner" spielt nicht mit
///     RAD_GAME_ERROR_STARTED       der erste Zug ist schon beendet
///     RAD_GAME_ERROR_INVALID_UNIT  "values" ist NULL oder passt nicht in die
///                                  festen Felder (RAD_WorldAddReserveUnit)
///     RAD_GAME_ERROR_FULL          kein Slot mehr frei (RAD_MAX_UNITS)
///
/// Das Spiel aendert sich bei keinem dieser Fehler.
///
RAD_GameResult_t RAD_GameAddUnit(RAD_Game_t *game, RAD_UserId_t owner, const RAD_Unit_t *values, RAD_UnitId_t *id);

///
/// Darf "user" seine Einheit "unit" aus der Reserve auf (x, y) stellen? Die Regel
/// hinter RAD_COMMAND_TYPE_DEPLOY_UNIT, an einer Stelle: der Server fragt sie, bevor
/// er das Kommando annimmt, und das Spiel noch einmal, bevor es schreibt.
///
///     RAD_GAME_ERROR_NO_USER         "user" ist RAD_USER_NONE
///     RAD_GAME_ERROR_NOT_PLAYING     "user" spielt nicht mit
///     RAD_GAME_ERROR_NO_UNIT         die Einheit gibt es nicht
///     RAD_GAME_ERROR_NOT_OWNED       sie gehoert nicht genau "user"
///     RAD_GAME_ERROR_NOT_IN_RESERVE  sie steht schon oder ist zerstoert
///     RAD_GAME_ERROR_OUT_OF_BOUNDS   (x, y) liegt ausserhalb der Welt
///     RAD_GAME_ERROR_NO_GROUND       das Feld hat kein Gelaende
///     RAD_GAME_ERROR_OCCUPIED        auf dem Feld steht schon eine Einheit
///
/// **Strenger als RAD_GameMayControlUnit.** Dort darf jeder eine herrenlose Figur
/// anfassen. Eine Einheit aus der Reserve kam aber mit einer Armee, und aufstellen
/// darf sie nur, wem die Armee gehoert.
///
/// Wer dran ist, prueft diese Regel nicht: das gilt fuer jedes Kommando und steht
/// beim Server (RAD_ControlExecuteCommand). Kosten hat das Aufstellen keine.
///
RAD_GameResult_t RAD_GameCheckDeployUnit(const RAD_Game_t *game, RAD_UserId_t user, RAD_UnitId_t unit, int32_t x, int32_t y);

///
/// **Den Zustand lesen: das steht bei den Typen, nicht hier.**
///
///     tile.h      RAD_GameNumberOfTiles, RAD_GameTileAt
///     unit.h      RAD_GameNumberOfUnits, RAD_GameUnitAt,
///                 RAD_GameNumberOfUserUnits, RAD_GameUserUnitAt
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
/// Die Fabriken fuellen ein Kommando aus und fuehren nichts aus: Art,
/// Sequenznummer und Absender kommen aus dem Spiel, alles andere aus den
/// Argumenten.
///
/// RAD_GameMoveUnit nimmt den Weg als Ganzes (model/path/path.h) -- wo er
/// anfaengt, sagt die Figur und nicht der Aufrufer. Sie liefert false, wenn der
/// Pfad keiner ist: kein Zeiger, keine Schritte oder mehr als RAD_PATH_MAX_STEPS.
/// Dann bleibt "output" unberuehrt und die Sequenznummer stehen.
///
/// RAD_GameDeployUnit stellt eine Einheit aus der Reserve auf (x, y). Sie liefert
/// false fuer ein NULL-Spiel, ein NULL-"output" und eine Id unter 0
/// (RAD_UNIT_NONE) -- auch dann bleiben "output" und die Sequenznummer stehen. Ob
/// die Einheit wirklich aufgestellt werden kann, ist eine Frage an den Zustand und
/// entscheidet sich beim Ausfuehren (RAD_GameCheckDeployUnit).
///
bool RAD_GameDeployUnit(RAD_Game_t *game, RAD_UnitId_t unit, int16_t x, int16_t y, RAD_Command_t *output);
bool RAD_GameDestroyUnit(RAD_Game_t *game, RAD_UnitId_t id, RAD_Command_t *output);
bool RAD_GameMoveUnit(RAD_Game_t *game, RAD_UnitId_t id, const RAD_Path_t *path, RAD_Command_t *output);
bool RAD_GameShoot(RAD_Game_t *game, RAD_UnitId_t id, int16_t x, int16_t y, RAD_Command_t *output);

void RAD_GameExecuteCommand(RAD_Game_t *game, RAD_Command_t *command);
void RAD_GameRollbackLastCommand(RAD_Game_t *game);

///
/// Weltdefinition -- die Welt, bevor gespielt wird.
///
/// **Eine Funktion und ein Pfad, kein Puffer und kein Format.** Wie eine
/// Weltdefinition aussieht, ist Sache des Moduls: der Parser und der Leser liegen
/// hinter src/include/ und kommen hier nicht vor. Wer eine Welt laden will, nennt
/// die Datei -- alles andere geschieht drinnen.
///
/// Das ist der Grund, aus dem das Lesen ueberhaupt in dieses Modul gehoert
/// (CMakeLists.txt): es schreibt in die Welt, und die steht privat. Waere es eine
/// Bibliothek daneben, muesste der Spielzustand fuer sie -- und damit fuer jeden --
/// offenliegen.
///
typedef enum
{
    RAD_GAME_LOAD_OK = 0,

    ///
    /// Die Datei.
    ///
    /// NOT_FOUND ist "nicht zu oeffnen" und nicht "gibt es nicht": ein fehlendes
    /// Leserecht sieht von hier genauso aus, und die Unterscheidung braucht der
    /// Aufrufer nicht -- geladen wird in beiden Faellen nichts.
    ///
    RAD_GAME_LOAD_ERROR_NOT_FOUND,
    RAD_GAME_LOAD_ERROR_UNREADABLE,

    ///
    /// Groesser als die Grenze, die intern steht. Eine Weltdefinition dieses
    /// Spiels liegt weit darunter; wer sie erreicht, hat keine gegeben.
    ///
    RAD_GAME_LOAD_ERROR_TOO_LARGE,
    RAD_GAME_LOAD_ERROR_OUT_OF_MEMORY,

    ///
    /// Der Inhalt. Von "ist das ueberhaupt JSON" bis zu "widerspricht sich die
    /// Datei selbst" -- in dieser Reihenfolge wird auch geprueft, damit eine Datei
    /// aus einer anderen Version "andere Version" meldet und nicht einen Fehler in
    /// ihren Zeilen.
    ///
    RAD_GAME_LOAD_ERROR_SYNTAX,
    RAD_GAME_LOAD_ERROR_SCHEMA,
    RAD_GAME_LOAD_ERROR_VERSION,

    ///
    /// Die Welt in der Datei hat Abmessungen, die dieses Programm nicht halten
    /// kann -- groesser als seine Obergrenze oder leer --, oder ihre Zeilen passen
    /// nicht zueinander.
    ///
    RAD_GAME_LOAD_ERROR_WORLD_SIZE,

    RAD_GAME_LOAD_ERROR_TILE_TYPE,

    ///
    /// Die Datei widerspricht sich selbst -- eine Hoehe auf einem Feld ohne
    /// Gelaende.
    ///
    RAD_GAME_LOAD_ERROR_INCONSISTENT,

    ///
    /// Im Spiel stehen schon Figuren. Eine Welt wird nur ersetzt, solange keine
    /// darauf steht (unten).
    ///
    RAD_GAME_LOAD_ERROR_WORLD_OCCUPIED
} RAD_GameLoadResult_t;

///
/// Macht daraus einen Text zum Loggen, wie RAD_GameResultText. Immer ein gueltiger
/// Zeiger, auch bei einem Wert ausserhalb der Aufzaehlung.
///
const char* RAD_GameLoadResultText(RAD_GameLoadResult_t result);

///
/// Liest aus "path" Gelaende und Hoehen einer Welt und ersetzt damit die Welt von
/// "game"; die Groesse der Welt kommt aus der Datei. Das Format beschreibt
/// game/schema/world.schema.json, die Regeln, die ein Schema nicht fassen kann,
/// stehen in world_definition.h.
///
/// Die Datei beschreibt nur die Welt -- keine Figuren, keinen Besitz, keinen Zug.
/// Gelesen wird sie einmal, beim Aufbau eines Spiels, und danach nicht mehr
/// gebraucht.
///
/// **Nur eine Welt ohne Figuren.** Stehen schon welche im Spiel, liefert sie
/// RAD_GAME_LOAD_ERROR_WORLD_OCCUPIED und aendert nichts: was mit einer Figur
/// geschieht, deren Feld es in der neuen Welt nicht gibt, ist eine Regel, die es
/// nicht gibt -- dieselbe Haltung wie beim Entfernen eines Tiles unter einer Figur.
///
/// Ein misslungener Ladevorgang laesst das Spiel unangetastet und meldet nichts,
/// ein gelungener meldet genau die Felder, die sich geaendert haben -- auch die,
/// die mit einer kleineren Welt wegfallen (removed).
///
RAD_GameLoadResult_t RAD_LoadWorldFromFile(RAD_Game_t *game, const char *path);

#endif
