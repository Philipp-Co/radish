#ifndef __RAD_CONTROL_LOADER_SAVE_FILE_H__
#define __RAD_CONTROL_LOADER_SAVE_FILE_H__

#include <radish/game/game.h>
#include <radish/serialization/serialization.h>

///
/// control/loader/save_file -- woher der Text kommt: Datei aufmachen, Bytes
/// holen, an die Serialisierung geben.
///
/// Privat wie die anderen Module unter control/: der Header liegt neben seiner
/// Quelle, und nur loader.c bindet ihn ein. Von aussen fuehrt der Weg zum
/// Spielstand ueber RAD_ControlCreateGame und sonst nirgendwo entlang.
///
/// Das Format steht nicht hier, sondern in
/// <radish/serialization/serialization.h> -- dort ist beschrieben, wie ein
/// Spielstand aussieht, und dort wird er gelesen. Dieses Modul ist der Adapter
/// darauf und duenn mit Absicht: ihm bleiben genau die Fragen der Datei -- gibt
/// es sie, laesst sie sich lesen, ist sie nicht zu gross.
///
/// Es entscheidet auch nichts. Ob ein misslungener Ladevorgang den Server
/// anhaelt oder ein leeres Spiel stehen laesst, ist eine Frage des Betriebs und
/// wird in loader.c beantwortet; hier wird nur gemeldet, was war.
///

///
/// Nur die Datei. Was am Inhalt falsch war, sagt RAD_SerializeResult_t -- das
/// geht unveraendert durch, statt hier nachgebaut zu werden. Ein zweites Enum
/// daneben waere entweder ungenauer oder eine Kopie; dieselbe Ueberlegung wie bei
/// interface/command.h und dem Ergebnis des Codecs.
///
typedef enum
{
    RAD_SAVE_FILE_OK = 0,

    /// Die Datei laesst sich nicht oeffnen.
    RAD_SAVE_FILE_ERROR_NOT_FOUND,

    /// Sie laesst sich nicht bis zum Ende lesen.
    RAD_SAVE_FILE_ERROR_UNREADABLE,

    ///
    /// Groesser als RAD_SAVE_JSON_MAX. Ein Spielstand dieses Spiels ist es dann
    /// nicht mehr -- die Grenze steht in serialization.h und ist aus der Groesse
    /// der Welt gerechnet.
    ///
    RAD_SAVE_FILE_ERROR_TOO_LARGE,

    /// Kein Speicher fuer den Inhalt.
    RAD_SAVE_FILE_ERROR_OUT_OF_MEMORY,

    /// Gelesen, aber die Serialisierung hat sie abgelehnt -- Grund in "out_reason".
    RAD_SAVE_FILE_ERROR_CONTENT
} RAD_SaveFileResult_t;

///
/// Macht daraus einen Text zum Loggen. Immer ein gueltiger Zeiger, auch bei einem
/// Wert ausserhalb der Aufzaehlung.
///
const char* RAD_SaveFileResultText(RAD_SaveFileResult_t result);

///
/// Liest den Spielstand aus "path" in "game".
///
/// "game" muss ein angelegtes Spiel sein und wird gefuellt, nicht ersetzt: die
/// Serialisierung uebernimmt daraus alles, was nicht im Spielstand steht -- die
/// Ereignisverwaltung, den Absender, die Sequenznummer. Bei jedem Fehler bleibt
/// es unberuehrt, ein misslungener Ladevorgang laesst also genau das stehen, was
/// vorher darin war.
///
/// "out_reason" nimmt das Ergebnis der Serialisierung auf und darf NULL sein; es
/// wird nur bei RAD_SAVE_FILE_ERROR_CONTENT beschrieben.
///
RAD_SaveFileResult_t RAD_ReadGameFromSaveFile(const char *path, RAD_Game_t *game, RAD_SerializeResult_t *out_reason);

#endif
