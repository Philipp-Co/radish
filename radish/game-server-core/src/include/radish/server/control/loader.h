#ifndef __RAD_CONTROL_LOADER_H__
#define __RAD_CONTROL_LOADER_H__

#include <radish/game/game.h>
#include <radish/game/control/events/event_manager.h>

typedef struct
{
   RAD_EventsTileChangedCallback_t tile_changed; 
} RAD_EventCallbacks_t;

///
/// control/loader -- woher das Spiel kommt, an dem der Server arbeitet.
///
/// Es gehoert zu control/, weil es dieselbe Frage von der anderen Seite
/// beantwortet: execute.h entscheidet, was mit dem Spielzustand geschieht, und
/// hier entsteht der Zustand, um den es geht. Zusammengehalten wird beides von
/// main, das das Spiel hier holt und damit die Steuerung fuettert
/// (RAD_CreateControl).
///
///     RAD_ControlCreateGame ──► RAD_ControlGame_t ──► RAD_CreateControl ──► execute
///
/// "Erzeugen" und "Laden" ist hier dasselbe: heute entsteht ein Spiel, spaeter
/// wird eines eingelesen -- aus einer Datei ueber die Serialisierung in
/// radish_game oder aus dem, was ein Spielstand sonst ist. Fuer den Aufrufer
/// aendert das nichts, und genau deshalb steht diese Grenze zwischen ihm und der
/// Herkunft.
///
/// Zwei Wege hinein: ohne Pfad entsteht ein leeres Spiel, so wie RAD_CreateGame
/// es hergibt -- ein Raster aus Grund und die eine Figur, die RAD_InitWorld setzt.
/// Mit Pfad kommt RAD_LoadGameFromFile darueber, und damit ist der Spielstand fuer
/// diese Datei eine Zeile: Datei aufmachen, Format erkennen, Inhalt pruefen und
/// einsetzen macht das Spielmodul (radish/game/game.h). Ein eigenes Modul dafuer
/// gab es hier, solange die Serialisierung nur Puffer kannte -- seit sie Pfade
/// nimmt, waere es eine Weiterleitung ohne Inhalt.
///

///
/// Das Spiel des Servers und das, was mit ihm zusammen lebt.
///
/// **Zwei Zeiger und ein Lebenslauf.** Ein Spiel haengt an einem Event-Manager, den
/// es nicht besitzt: RAD_CreateGame hinterlegt ihn nur, und RAD_DestroyGame baut ihn
/// nicht ab (radish/game/game.h). Wer ein Spiel anlegt, hat also zwei Dinge in der
/// Hand, die genau gleich lange leben und in einer festen Reihenfolge weggehen --
/// und das steht hier im Typ, damit es nicht in jedem Aufrufer stehen muss.
///
/// **Warum nicht bloss das Spiel?** Weil sich der Manager aus ihm nicht
/// zurueckholen laesst. RAD_Game_t ist von aussen ein Name ohne Inhalt, und die
/// Fassade des Spielmoduls gibt ihn nicht heraus -- ein Abbauen, das nur den
/// Spielzeiger bekommt, muesste ihn also erraten. Vorher stand hier genau das und
/// las das Feld direkt aus der Struktur; seit sie privat ist, geht das nicht mehr,
/// und richtig war es auch davor nicht.
///
typedef struct
{
    ///
    /// Das Spiel. NULL heisst: es gibt keines -- alles andere ist ein fertiges,
    /// in das geladen wurde oder das leer anfaengt.
    ///
    RAD_Game_t *game;

    ///
    /// Der Event-Manager, an dem es haengt. Sache dieses Moduls: er steht hier,
    /// weil RAD_ControlDestroyGame ihn braucht, und nicht, damit ein Aufrufer ihn
    /// benutzt. Wer Ereignisse abonnieren will, tut es ueber ihn -- wer ihn abbauen
    /// will, nicht: das tut RAD_ControlDestroyGame und sonst niemand.
    ///
    RAD_EventManager_t *event_manager;
} RAD_ControlGame_t;

///
/// Legt das Spiel des Servers an.
///
/// "save_path" ist der Spielstand, der geladen werden soll, oder NULL fuer ein
/// leeres Spiel.
///
/// "game" im Ergebnis ist NULL, wenn keines zustande kam: kein Speicher, oder der
/// Spielstand liess sich nicht lesen -- ein angegebener Pfad, der nicht traegt,
/// ist ein Abbruchgrund und kein Anlass, stillschweigend leer weiterzumachen. Der
/// Grund steht dann schon im Log; der Aufrufer muss ihn nicht auch noch erfahren.
/// Das Ergebnis laesst sich in diesem Fall trotzdem an RAD_ControlDestroyGame
/// geben, es ist dann nur nichts zu tun.
///
/// Was zu einem Spiel gehoert, entsteht mit ihm: der Event-Manager, an dem es
/// haengt, und die Welt darin. Nichts davon muss der Aufrufer stellen oder selbst
/// abbauen -- das ist der Unterschied zu RAD_CreateGame aus radish_game, das
/// beides erwartet. Er traegt nur beide Zeiger zusammen weiter, und wozu der
/// zweite gut ist, muss er nicht wissen.
///
/// Der Absender, den das Spiel selbst erzeugten Kommandos gibt (local_user), ist
/// RAD_USER_NONE: der Server sitzt an keinem Client. Ein geladener Spielstand
/// aendert das nicht -- gespeichert wird die Welt, nicht das laufende Programm.
///
RAD_ControlGame_t RAD_ControlCreateGame(const char *save_path, RAD_EventCallbacks_t *callbacks);

///
/// Gibt beides wieder her und setzt beide Zeiger auf NULL, wie RAD_DestroyGame.
/// Ein Spiel, das schon NULL ist, ist kein Fehler, und ein halb entstandenes Paar
/// auch nicht.
///
/// Die Reihenfolge steht hier und nicht beim Aufrufer: erst das Spiel, dann sein
/// Event-Manager. Das Spiel haelt einen Zeiger auf ihn, also muss er es
/// ueberleben -- andersherum griffe RAD_DestroyGame auf Speicher zu, den es nicht
/// mehr gibt.
///
/// Es muss nach der Steuerung abgebaut werden, die sich das Spiel nur geliehen hat
/// (RAD_DestroyControl).
///
void RAD_ControlDestroyGame(RAD_ControlGame_t *game);

#endif
