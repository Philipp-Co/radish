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
bool RAD_GameShoot(RAD_Game_t *game, RAD_EntityId_t id, int16_t x, int16_t y, RAD_Command_t *output);

void RAD_GameExecuteCommand(RAD_Game_t *game, RAD_Command_t *command);
void RAD_GameRollbackLastCommand(RAD_Game_t *game);

///
/// Spielstand -- ein Spiel in eine Datei und zurueck.
///
/// **Zwei Funktionen und ein Pfad, kein Puffer und kein Format.** Wie ein
/// Spielstand aussieht, ist Sache des Moduls: das Format, der JSON-Schreiber, der
/// Parser und die vier Serialisierer liegen hinter src/include/ und kommen hier
/// nicht vor. Wer ein Spiel sichert, nennt die Datei -- alles andere geschieht
/// drinnen.
///
/// Das ist der Grund, aus dem die Serialisierung ueberhaupt in dieses Modul
/// gehoert (CMakeLists.txt): sie liest und schreibt jedes Feld von Welt und Spiel,
/// und die stehen privat. Waere sie eine Bibliothek daneben, muesste der
/// Spielzustand fuer sie -- und damit fuer jeden -- offenliegen. So bleibt von
/// ihr nach aussen genau das uebrig, was ein Aufrufer wirklich braucht: speichern,
/// laden, und was dabei schiefging.
///
/// **Der Weg zurueck fuellt, er legt nicht an.** RAD_LoadGameFromFile braucht ein
/// Spiel, das es schon gibt: ein Spielstand beschreibt die Welt, aber nicht die
/// Ereignisverwaltung, nicht den Absender und nicht die Sequenznummer -- die
/// kommen aus RAD_CreateGame und bleiben beim Laden stehen. Ein Spiel aus einer
/// Datei allein waere eines ohne Abonnenten.
///
/// **Ein misslungener Ladevorgang laesst das Spiel unangetastet.** Geschrieben
/// wird erst, wenn die Datei gelesen, das Format erkannt und der Inhalt vollstaendig
/// geprueft ist -- bis dahin steht alles in einem eigenen Puffer. Wer in ein
/// laufendes Spiel laedt und einen Fehler bekommt, spielt unveraendert weiter.
///
typedef enum
{
    RAD_GAME_SAVE_OK = 0,

    ///
    /// Die Datei.
    ///
    /// NOT_FOUND ist "nicht zu oeffnen" und nicht "gibt es nicht": ein fehlendes
    /// Leserecht sieht von hier genauso aus, und die Unterscheidung braucht der
    /// Aufrufer nicht -- geladen wird in beiden Faellen nichts.
    ///
    RAD_GAME_SAVE_ERROR_NOT_FOUND,
    RAD_GAME_SAVE_ERROR_UNREADABLE,
    RAD_GAME_SAVE_ERROR_NOT_WRITABLE,

    ///
    /// Zu gross fuer einen Spielstand dieses Spiels, in beide Richtungen: beim
    /// Lesen ist die Datei groesser als die Grenze, beim Schreiben passt das Spiel
    /// nicht hinein. Ein Wert und nicht zwei, weil es dieselbe Grenze ist -- sie
    /// steht intern und ist aus der Groesse der Welt gerechnet.
    ///
    RAD_GAME_SAVE_ERROR_TOO_LARGE,
    RAD_GAME_SAVE_ERROR_OUT_OF_MEMORY,

    ///
    /// Der Inhalt. Von "ist das ueberhaupt JSON" bis zu "widerspricht sich die
    /// Datei selbst" -- in dieser Reihenfolge wird auch geprueft, damit eine fremde
    /// Datei "fremdes Format" meldet und nicht einen Strukturfehler tief in der
    /// Welt.
    ///
    RAD_GAME_SAVE_ERROR_SYNTAX,
    RAD_GAME_SAVE_ERROR_SCHEMA,
    RAD_GAME_SAVE_ERROR_FORMAT,
    RAD_GAME_SAVE_ERROR_VERSION,

    /// Die Welt im Spielstand hat andere Abmessungen als die dieses Programms.
    RAD_GAME_SAVE_ERROR_WORLD_SIZE,

    RAD_GAME_SAVE_ERROR_TILE_TYPE,
    RAD_GAME_SAVE_ERROR_ENTITY_TYPE,
    RAD_GAME_SAVE_ERROR_ENTITY_ID,
    RAD_GAME_SAVE_ERROR_ENTITY_POSITION,
    RAD_GAME_SAVE_ERROR_TILE_OCCUPIED,

    ///
    /// Die Datei widerspricht sich selbst -- etwa wenn ein Tile auf eine andere
    /// Entitaet zeigt als die, die dort laut ihrer eigenen Position steht.
    ///
    RAD_GAME_SAVE_ERROR_INCONSISTENT
} RAD_GameSaveResult_t;

///
/// Macht daraus einen Text zum Loggen, wie RAD_GameResultText. Immer ein gueltiger
/// Zeiger, auch bei einem Wert ausserhalb der Aufzaehlung.
///
const char* RAD_GameSaveResultText(RAD_GameSaveResult_t result);

///
/// Schreibt das Spiel nach "path". Eine vorhandene Datei wird ersetzt.
///
/// Eingerueckt und mit Zeilenumbruechen, denn ein Spielstand wird von Hand gelesen
/// und geaendert -- er ist die einzige Datei dieses Spiels, in der etwas steht, das
/// jemand nachschlagen will. Der Platz dafuer ist eingerechnet.
///
RAD_GameSaveResult_t RAD_SaveGameToFile(const RAD_Game_t *game, const char *path);

///
/// Liest den Spielstand aus "path" in "game", das ein angelegtes Spiel sein muss.
///
RAD_GameSaveResult_t RAD_LoadGameFromFile(RAD_Game_t *game, const char *path);

#endif
