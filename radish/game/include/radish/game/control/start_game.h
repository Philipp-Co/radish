#ifndef __RAD_GAME_CONTROL_START_GAME_H__
#define __RAD_GAME_CONTROL_START_GAME_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <radish/game/game.h>
#include <radish/game/user.h>
#include <radish/game/model/unit/unit.h>

///
/// Spielstart -- wie aus einer Spieldatei ein Spiel wird.
///
/// Das Django des Game-Servers legt die Datei beim Start eines Spiels ab
/// (radish/game-server/instances/views.py), das Format beschreibt
/// game/schema/spielstart.schema.json: zwei Spieler, jeder mit Keycloak-Name,
/// Spieler-Kennung und vollstaendig aufgeloester Armee.
///
/// **Zwei Schritte, beide hier.** Erst wird die Datei gelesen und geprueft
/// (RAD_LoadGameStart, RAD_ParseGameStart) -- heraus kommt ein RAD_GameStart_t,
/// der noch nichts am Spiel aendert. Dann wird daraus das Spiel eingerichtet
/// (RAD_StartGame): die Spieler treten bei, ihre Einheiten entstehen im
/// Einheitenpool des Spiels und kommen in Einheitenliste und Reserve ihres
/// Spielers. RAD_StartGameFromFile macht beides in einem Aufruf.
///
/// Vorher lag das Lesen im Server (game-server-core) und das Spielmodul kannte die
/// Datei nicht. Es liegt jetzt hier, weil das Einrichten an den Strukturen des
/// Spiels haengt -- Pool, Spieler, Reserve --, die von aussen nicht zu sehen sind.
///
/// **Die Armee wird vollstaendig gelesen.** Jede Einheit landet als RAD_Unit_t
/// mit ihren Werten und Mitgliedern (radish/game/model/unit/unit.h); beim
/// Einrichten wird sie in den Pool uebernommen, ohne Umrechnung. Die Ausruestung
/// wird geprueft und nicht uebernommen: sie traegt bisher nur einen Namen und
/// wirkt im Spiel nicht.
///
/// **Was das Schema verlangt, verlangt auch der Leser.** Ein fehlender
/// Pflichtschluessel, ein leerer Name, ein Wert ausserhalb 0..32767 oder eine
/// unbekannte Waffenklasse ist RAD_GAME_START_ERROR_SCHEMA. Was das Schema
/// erlaubt, die festen Felder des Spiels aber nicht halten koennen, ist
/// RAD_GAME_START_ERROR_LIMIT -- abgelehnt wird dann die ganze Datei, nichts
/// wird gekuerzt.
///
/// Unbekannte Schluessel werden uebergangen: die Datei schreibt das Backend und
/// nicht ein Mensch, und ein Feld, das es spaeter dazubekommt, soll einen aelteren
/// Server nicht aufhalten.
///
#define RAD_GAME_START_NUMBER_OF_PLAYERS 2

/// Platz fuer Namen (Spieler, Armee) samt abschliessender Null -- Keycloak und das
/// Backend lassen 255 Zeichen zu.
#define RAD_GAME_START_NAME_MAX 256

/// Die Spieler-Kennung des Backends: genau 8 Zeichen, dazu die Null.
#define RAD_GAME_START_IDENTIFIER_LENGTH 8

///
/// Einheiten je Armee. Zwei volle Armeen fuellen den Einheitenpool des Spiels
/// genau (RAD_MAX_UNITS, 64); das Backend muss dieselbe Grenze pruefen.
///
#define RAD_GAME_START_MAX_UNITS 32

///
/// Obergrenze der Datei. Zwei Armeen haben keine feste Obergrenze an Einheiten, und
/// jede Entitaet traegt ihre Waffen mit allen Werten -- ein Megabyte reicht fuer
/// Armeen weit jenseits dessen, was ein Punktelimit zulaesst.
///
#define RAD_GAME_START_FILE_MAX (1024 * 1024)

typedef struct
{
    char name[RAD_GAME_START_NAME_MAX];
    char identifier[RAD_GAME_START_IDENTIFIER_LENGTH + 1];
    char army_name[RAD_GAME_START_NAME_MAX];

    ///
    /// Die Einheiten in der Reihenfolge der Armee, mit den Werten aus der Datei.
    /// Id, Zustand, Besitzer und Position sind nicht gesetzt -- die vergibt das
    /// Spiel beim Einrichten (RAD_StartGame).
    ///
    RAD_Unit_t units[RAD_GAME_START_MAX_UNITS];
    int32_t number_of_units;
} RAD_GameStartPlayer_t;

///
/// Zwei Armeen mit allen Werten sind gross -- gut 150 KB. Ein Spielstart gehoert
/// deshalb auf den Heap und nicht in einen Aufrufrahmen:
/// RAD_CreateGameStart legt einen leeren an, RAD_DestroyGameStart
/// gibt ihn wieder her.
///
typedef struct
{
    /// In der Reihenfolge der Datei: zuerst der Host, dann der zweite Spieler.
    RAD_GameStartPlayer_t players[RAD_GAME_START_NUMBER_OF_PLAYERS];
} RAD_GameStart_t;

typedef enum
{
    RAD_GAME_START_OK = 0,

    /// Die Datei: nicht zu oeffnen, nicht zu lesen, zu gross, kein Speicher.
    RAD_GAME_START_ERROR_NOT_FOUND,
    RAD_GAME_START_ERROR_UNREADABLE,
    RAD_GAME_START_ERROR_TOO_LARGE,
    RAD_GAME_START_ERROR_OUT_OF_MEMORY,

    ///
    /// Der Inhalt: kein JSON, oder nicht der erwartete Aufbau -- nicht genau zwei
    /// Spieler, ein leerer oder zu langer Name, eine Kennung, die nicht genau
    /// RAD_GAME_START_IDENTIFIER_LENGTH Zeichen hat, eine Armee ohne
    /// Einheiten, eine Einheit ohne Entitaeten, ein fehlender Pflichtschluessel,
    /// ein Wert vom falschen Typ oder ausserhalb 0..32767, eine unbekannte
    /// Waffenklasse.
    ///
    RAD_GAME_START_ERROR_SYNTAX,
    RAD_GAME_START_ERROR_SCHEMA,

    ///
    /// Nach dem Schema in Ordnung, aber mehr, als das Spiel halten kann: mehr als
    /// RAD_GAME_START_MAX_UNITS Einheiten, mehr als RAD_UNIT_MAX_MEMBERS
    /// Mitglieder, mehr als RAD_UNIT_MAX_WEAPONS Waffen, oder ein Name von
    /// Einheitentyp, Profil oder Waffe, der nicht in RAD_UNIT_NAME_MAX passt.
    ///
    RAD_GAME_START_ERROR_LIMIT,

    ///
    /// Die Datei war in Ordnung, das Spiel hat den Start aber abgelehnt -- nur
    /// aus RAD_StartGameFromFile; den Grund nennt dort "game_result".
    ///
    RAD_GAME_START_ERROR_REJECTED
} RAD_GameStartResult_t;

///
/// Macht daraus einen Text zum Loggen. Immer ein gueltiger Zeiger, auch bei einem
/// Wert ausserhalb der Aufzaehlung.
///
const char* RAD_GameStartResultText(RAD_GameStartResult_t result);

///
/// Legt einen leeren Spielstart auf dem Heap an, NULL ohne Speicher. Freigegeben
/// wird er mit RAD_DestroyGameStart, das den Zeiger auf NULL setzt und
/// einen, der schon NULL ist, hinnimmt.
///
RAD_GameStart_t* RAD_CreateGameStart(void);
void RAD_DestroyGameStart(RAD_GameStart_t **start);

///
/// Liest die Spielstart-Datei aus "path" nach "start". Bei einem Fehler ist
/// "start" unberuehrt.
///
RAD_GameStartResult_t RAD_LoadGameStart(const char *path, RAD_GameStart_t *start);

///
/// Dasselbe aus einem Puffer -- fuer die Tests, und fuer RAD_LoadGameStart
/// selbst, nachdem es die Datei gelesen hat. "json" muss nicht nullterminiert sein.
///
RAD_GameStartResult_t RAD_ParseGameStart(const char *json, size_t length, RAD_GameStart_t *start);

///
/// Die Id, unter der ein Spieler im Spiel steht, aus seiner Kennung.
///
/// **Gepackt, nicht nachgeschlagen.** Die acht Zeichen der Kennung sind die acht
/// Bytes der Id, das erste Zeichen im hoechsten Byte: "aB3xK9pQ" ist
/// 0x614233784B397051 -- Zeichen fuer Zeichen 'a' = 0x61, 'B' = 0x42 und so
/// weiter. Es braucht damit keine Tabelle und keinen Zustand, und jeder, der die
/// Kennung kennt, rechnet dieselbe Id aus, ohne den Server zu fragen -- das
/// Backend in Python mit int.from_bytes(kennung.encode("ascii"), "big").
///
/// RAD_USER_NONE (0) kommt dabei nicht heraus: kein Zeichen einer Kennung ist 0.
/// RAD_USER_NONE liefert die Funktion nur fuer eine Kennung, die nicht genau
/// RAD_GAME_START_IDENTIFIER_LENGTH Zeichen aus [A-Za-z0-9] hat.
///
RAD_UserId_t RAD_UserIdFromIdentifier(const char *identifier);

///
/// Der Rueckweg, fuer das Log: schreibt die Kennung zur Id nach "out" (samt Null).
/// false und ein leerer String, wenn die Id keine gepackte Kennung ist.
///
bool RAD_IdentifierFromUserId(RAD_UserId_t user, char out[RAD_GAME_START_IDENTIFIER_LENGTH + 1]);

///
/// Richtet "game" nach "start" ein, genau einmal:
///
///   1. Beide Spieler treten bei (RAD_GameAddPlayer), in der Reihenfolge der
///      Datei -- das ist zugleich die Zugreihenfolge, der Host zieht zuerst. Wer
///      schon mitspielt, bleibt an seiner Stelle: im Server tritt bei, wer sein
///      erstes Kommando schickt, und das kann vor dem Start ankommen.
///   2. Jede Einheit ihrer Armee entsteht im Einheitenpool des Spiels, mit den
///      Werten aus der Datei, dem Spieler als Besitzer (RAD_Unit_t.owner) und
///      im Zustand RAD_UNIT_STATE_RESERVE.
///   3. Sie kommt in die Einheitenliste ihres Spielers und in seine Reserve.
///
/// Abgelehnt wird, bevor sich etwas aendert:
///
///   - RAD_GAME_ERROR_INVALID_UNIT fuer ein NULL-Spiel oder einen NULL-Start,
///   - RAD_GAME_ERROR_STARTED, wenn das Spiel schon laeuft oder schon Einheiten
///     im Pool hat -- ein Spiel bekommt seine Armeen einmal,
///   - RAD_GAME_ERROR_NO_USER fuer eine Kennung, aus der keine Id wird,
///   - RAD_GAME_ERROR_FULL, wenn die Einheiten beider Armeen nicht in den Pool
///     passen.
///
/// Danach kann nur noch der Beitritt scheitern (kein Speicher fuer den Spieler,
/// RAD_GAME_ERROR_FULL). Wer erst mit diesem Start beigetreten ist, tritt wieder
/// aus -- auch dann bleibt das Spiel, wie es war. Einheiten entstehen erst, wenn beide
/// Spieler stehen, und koennen nach den Pruefungen oben nicht mehr scheitern.
///
/// Der Pool ist derselbe, den die Welt fuehrt (world.h): eine gestartete Einheit
/// laesst sich danach aufstellen (RAD_COMMAND_TYPE_DEPLOY_UNIT) und verlaesst
/// dabei die Reserve ihres Spielers.
///
RAD_GameResult_t RAD_StartGame(RAD_Game_t *game, const RAD_GameStart_t *start);

///
/// Beides in einem: liest "path" (RAD_LoadGameStart) und richtet "game" danach
/// ein (RAD_StartGame). Ein Fehler beim Lesen kommt als Ergebnis zurueck, und
/// das Spiel bleibt unberuehrt; lehnt das Spiel ab, ist es
/// RAD_GAME_START_ERROR_REJECTED, und "game_result" nennt den Grund. "game_result"
/// darf NULL sein; sonst steht darin immer das Ergebnis von RAD_StartGame, oder
/// RAD_GAME_OK, wenn es dazu nicht kam.
///
RAD_GameStartResult_t RAD_StartGameFromFile(RAD_Game_t *game, const char *path, RAD_GameResult_t *game_result);

#endif
