#ifndef __RAD_CONTROL_LOADER_H__
#define __RAD_CONTROL_LOADER_H__

#include <radish/game/game.h>

///
/// control/loader -- woher das Spiel kommt, an dem der Server arbeitet.
///
/// Es gehoert zu control/, weil es dieselbe Frage von der anderen Seite
/// beantwortet: execute.h entscheidet, was mit dem Spielzustand geschieht, und
/// hier entsteht der Zustand, um den es geht. Zusammengehalten wird beides von
/// main, das das Spiel hier holt und damit die Steuerung fuettert
/// (RAD_CreateControl).
///
///     RAD_ControlCreateGame ──► RAD_Game_t ──► RAD_CreateControl ──► execute
///
/// "Erzeugen" und "Laden" ist hier dasselbe: heute entsteht ein Spiel, spaeter
/// wird eines eingelesen -- aus einer Datei ueber radish_serialization oder aus
/// dem, was ein Spielstand sonst ist. Fuer den Aufrufer aendert das nichts, und
/// genau deshalb steht diese Grenze zwischen ihm und der Herkunft.
///
/// Zwei Wege hinein: ohne Pfad entsteht ein leeres Spiel, so wie RAD_CreateGame
/// es hergibt -- ein Raster aus Grund und die eine Figur, die RAD_InitWorld setzt.
/// Mit Pfad wird ein Spielstand aus einer JSON-Datei gelesen, ueber
/// radish_serialization; die Datei selbst nimmt ein Modul im Innern
/// (loader/save_file.h), das ausser diesem hier niemand sieht.
///

///
/// Legt das Spiel des Servers an.
///
/// "save_path" ist der Spielstand, der geladen werden soll, oder NULL fuer ein
/// leeres Spiel.
///
/// NULL als Ergebnis heisst: es gibt kein Spiel. Kein Speicher, oder der
/// Spielstand liess sich nicht lesen -- ein angegebener Pfad, der nicht traegt,
/// ist ein Abbruchgrund und kein Anlass, stillschweigend leer weiterzumachen. Der
/// Grund steht dann schon im Log; der Aufrufer muss ihn nicht auch noch erfahren.
///
/// Was zu einem Spiel gehoert, entsteht mit ihm: der Event-Manager, an dem es
/// haengt, und die Welt darin. Nichts davon muss der Aufrufer stellen oder
/// laenger am Leben halten als das Spiel selbst -- das ist der Unterschied zu
/// RAD_CreateGame aus radish_game, das beides erwartet.
///
/// Der Absender, den das Spiel selbst erzeugten Kommandos gibt (local_user), ist
/// RAD_USER_NONE: der Server sitzt an keinem Client. Ein geladener Spielstand
/// aendert das nicht -- gespeichert wird die Welt, nicht das laufende Programm.
///
RAD_Game_t* RAD_ControlCreateGame(const char *save_path);

///
/// Gibt das Spiel wieder her und setzt den Zeiger auf NULL, wie RAD_DestroyGame.
/// Ein Spiel, das schon NULL ist, ist kein Fehler.
///
/// Es muss nach der Steuerung abgebaut werden, die es sich nur geliehen hat
/// (RAD_DestroyControl).
///
void RAD_ControlDestroyGame(RAD_Game_t **game);

#endif
