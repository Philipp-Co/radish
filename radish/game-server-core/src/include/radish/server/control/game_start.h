#ifndef __RAD_SERVER_CONTROL_GAME_START_H__
#define __RAD_SERVER_CONTROL_GAME_START_H__

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <radish/game/user.h>
#include <radish/game/model/unit/unit.h>

///
/// Spielstart -- wer mit welcher Armee antritt.
///
/// Das Django des Game-Servers legt die Datei beim Start eines Spiels ab
/// (radish/game-server/instances/views.py), das Format beschreibt
/// game/schema/spielstart.schema.json: zwei Spieler, jeder mit Keycloak-Name,
/// Spieler-Kennung und vollstaendig aufgeloester Armee.
///
/// **Sache des Servers, nicht des Spiels.** Woher die Spieler kommen und in welchem
/// Format, weiss nur der Server; das Spiel (radish_game) bekommt davon nichts zu
/// sehen. Der Server liest die Datei hier und richtet sein Spiel danach ueber
/// dessen Setter ein -- das Spielmodul kennt weder die Datei noch ihr Format.
///
/// **Die Armee wird vollstaendig gelesen.** Jede Einheit landet als RAD_Unit_t
/// mit ihren Werten und Mitgliedern (radish/game/model/unit/unit.h), so wie
/// RAD_GameAddUnit sie in die Reserve nimmt -- der Server muss danach nichts mehr
/// umrechnen (RAD_ControlStartGame, execute.h). Die Ausruestung wird geprueft und
/// nicht uebernommen: sie traegt bisher nur einen Namen und wirkt im Spiel nicht.
///
/// **Was das Schema verlangt, verlangt auch der Leser.** Ein fehlender
/// Pflichtschluessel, ein leerer Name, ein Wert ausserhalb 0..32767 oder eine
/// unbekannte Waffenklasse ist RAD_CONTROL_GAME_START_ERROR_SCHEMA. Was das Schema
/// erlaubt, die festen Felder des Spiels aber nicht halten koennen, ist
/// RAD_CONTROL_GAME_START_ERROR_LIMIT -- abgelehnt wird dann die ganze Datei, nichts
/// wird gekuerzt.
///
/// Unbekannte Schluessel werden uebergangen: die Datei schreibt das Backend und
/// nicht ein Mensch, und ein Feld, das es spaeter dazubekommt, soll einen aelteren
/// Server nicht aufhalten.
///
#define RAD_CONTROL_GAME_START_NUMBER_OF_PLAYERS 2

/// Platz fuer Namen (Spieler, Armee) samt abschliessender Null -- Keycloak und das
/// Backend lassen 255 Zeichen zu.
#define RAD_CONTROL_GAME_START_NAME_MAX 256

/// Die Spieler-Kennung des Backends: genau 8 Zeichen, dazu die Null.
#define RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH 8

///
/// Einheiten je Armee. Zwei volle Armeen fuellen den Einheitenpool des Spiels
/// genau (RAD_MAX_UNITS, 64); das Backend muss dieselbe Grenze pruefen.
///
#define RAD_CONTROL_GAME_START_MAX_UNITS 32

///
/// Obergrenze der Datei. Zwei Armeen haben keine feste Obergrenze an Einheiten, und
/// jede Entitaet traegt ihre Waffen mit allen Werten -- ein Megabyte reicht fuer
/// Armeen weit jenseits dessen, was ein Punktelimit zulaesst.
///
#define RAD_CONTROL_GAME_START_FILE_MAX (1024 * 1024)

typedef struct
{
    char name[RAD_CONTROL_GAME_START_NAME_MAX];
    char identifier[RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH + 1];
    char army_name[RAD_CONTROL_GAME_START_NAME_MAX];

    ///
    /// Die Einheiten in der Reihenfolge der Armee, mit den Werten aus der Datei.
    /// Id, Zustand, Besitzer und Position sind nicht gesetzt -- die vergibt das
    /// Spiel beim Aufnehmen (RAD_GameAddUnit).
    ///
    RAD_Unit_t units[RAD_CONTROL_GAME_START_MAX_UNITS];
    int32_t number_of_units;
} RAD_ControlGameStartPlayer_t;

///
/// Zwei Armeen mit allen Werten sind gross -- gut 150 KB. Ein Spielstart gehoert
/// deshalb auf den Heap und nicht in einen Aufrufrahmen:
/// RAD_ControlCreateGameStart legt einen leeren an, RAD_ControlDestroyGameStart
/// gibt ihn wieder her.
///
typedef struct
{
    /// In der Reihenfolge der Datei: zuerst der Host, dann der zweite Spieler.
    RAD_ControlGameStartPlayer_t players[RAD_CONTROL_GAME_START_NUMBER_OF_PLAYERS];
} RAD_ControlGameStart_t;

typedef enum
{
    RAD_CONTROL_GAME_START_OK = 0,

    /// Die Datei: nicht zu oeffnen, nicht zu lesen, zu gross, kein Speicher.
    RAD_CONTROL_GAME_START_ERROR_NOT_FOUND,
    RAD_CONTROL_GAME_START_ERROR_UNREADABLE,
    RAD_CONTROL_GAME_START_ERROR_TOO_LARGE,
    RAD_CONTROL_GAME_START_ERROR_OUT_OF_MEMORY,

    ///
    /// Der Inhalt: kein JSON, oder nicht der erwartete Aufbau -- nicht genau zwei
    /// Spieler, ein leerer oder zu langer Name, eine Kennung, die nicht genau
    /// RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH Zeichen hat, eine Armee ohne
    /// Einheiten, eine Einheit ohne Entitaeten, ein fehlender Pflichtschluessel,
    /// ein Wert vom falschen Typ oder ausserhalb 0..32767, eine unbekannte
    /// Waffenklasse.
    ///
    RAD_CONTROL_GAME_START_ERROR_SYNTAX,
    RAD_CONTROL_GAME_START_ERROR_SCHEMA,

    ///
    /// Nach dem Schema in Ordnung, aber mehr, als das Spiel halten kann: mehr als
    /// RAD_CONTROL_GAME_START_MAX_UNITS Einheiten, mehr als RAD_UNIT_MAX_MEMBERS
    /// Mitglieder, mehr als RAD_UNIT_MAX_WEAPONS Waffen, oder ein Name von
    /// Einheitentyp, Profil oder Waffe, der nicht in RAD_UNIT_NAME_MAX passt.
    ///
    RAD_CONTROL_GAME_START_ERROR_LIMIT
} RAD_ControlGameStartResult_t;

///
/// Macht daraus einen Text zum Loggen. Immer ein gueltiger Zeiger, auch bei einem
/// Wert ausserhalb der Aufzaehlung.
///
const char* RAD_ControlGameStartResultText(RAD_ControlGameStartResult_t result);

///
/// Legt einen leeren Spielstart auf dem Heap an, NULL ohne Speicher. Freigegeben
/// wird er mit RAD_ControlDestroyGameStart, das den Zeiger auf NULL setzt und
/// einen, der schon NULL ist, hinnimmt.
///
RAD_ControlGameStart_t* RAD_ControlCreateGameStart(void);
void RAD_ControlDestroyGameStart(RAD_ControlGameStart_t **start);

///
/// Liest die Spielstart-Datei aus "path" nach "start". Bei einem Fehler ist
/// "start" unberuehrt.
///
RAD_ControlGameStartResult_t RAD_ControlLoadGameStart(const char *path, RAD_ControlGameStart_t *start);

///
/// Dasselbe aus einem Puffer -- fuer die Tests, und fuer RAD_ControlLoadGameStart
/// selbst, nachdem es die Datei gelesen hat. "json" muss nicht nullterminiert sein.
///
RAD_ControlGameStartResult_t RAD_ControlParseGameStart(const char *json, size_t length, RAD_ControlGameStart_t *start);

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
/// RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH Zeichen aus [A-Za-z0-9] hat.
///
RAD_UserId_t RAD_ControlUserIdFromIdentifier(const char *identifier);

///
/// Der Rueckweg, fuer das Log: schreibt die Kennung zur Id nach "out" (samt Null).
/// false und ein leerer String, wenn die Id keine gepackte Kennung ist.
///
bool RAD_ControlIdentifierFromUserId(RAD_UserId_t user, char out[RAD_CONTROL_GAME_START_IDENTIFIER_LENGTH + 1]);

#endif
