#ifndef __RAD_SERVER_INTERFACE_GAME_REPORT_H__
#define __RAD_SERVER_INTERFACE_GAME_REPORT_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/game/user.h>
#include <radish/game/control/start_game.h>

///
/// interface/game_report -- das Ende eines Spiels an das Backend melden.
///
/// Der Server spricht das Backend nicht selbst an. Er startet ein Programm, das
/// es tut -- im Container docker/game-server/register_instance.py, das sich das
/// Token holt und POST api/servers/<name>/game-finished/ schickt:
///
///     <command> game-finished <winner> <loser>
///     <command> game-finished draw
///
/// Welches Programm, sagt die Umgebung (RADISH_GAME_FINISHED_COMMAND, main.c);
/// fehlt sie, wird nicht gemeldet. Das Programm erbt die Umgebung des Servers und
/// findet dort Backend-URL, Instanzname und Zugangsdaten.
///
/// **Gestartet, nicht abgewartet.** Ein Token holen und das Backend rufen kann
/// Sekunden dauern; die Spielschleife soll derweil weiterlaufen. Was aus einer
/// Meldung wurde, holt RAD_ReportCollectFinished ab und schreibt es ins Log --
/// aus der Hauptschleife gerufen, damit keine beendeten Prozesse liegen bleiben.
///
/// **posix_spawn statt system().** Die Argumente gehen als Array hinueber, ohne
/// Shell: eine Kennung wird nie als Befehl gelesen.
///

///
/// Was gemeldet wird: die Kennungen (Player.identifier im Backend, dieselben wie
/// in der Spielstart-Datei) von Sieger und Verlierer. Bei einem Unentschieden
/// sind beide leer ("draw" auf der Kommandozeile).
///
typedef struct
{
    char winner[RAD_GAME_START_IDENTIFIER_LENGTH + 1];
    char loser[RAD_GAME_START_IDENTIFIER_LENGTH + 1];
} RAD_GameReport_t;

///
/// Bildet die Meldung aus dem Ende des Spiels: "winner" wie RAD_GameWinner,
/// RAD_USER_NONE fuer ein Unentschieden, und die Mitspieler. Verloren hat der
/// Mitspieler, der nicht gewonnen hat.
///
/// false, wenn sich daraus keine Meldung bilden laesst: der Sieger spielt nicht
/// mit, es gibt keinen zweiten Mitspieler, oder eine Id hat keine Kennung
/// (RAD_IdentifierFromUserId). "out" bleibt dann unangetastet.
///
bool RAD_GameReportFromResult(RAD_UserId_t winner,
                              const RAD_UserId_t *players,
                              int32_t number_of_players,
                              RAD_GameReport_t *out);

///
/// Startet "command" mit der Meldung als Argumenten und kehrt sofort zurueck.
/// false, wenn es sich nicht starten liess (der Grund steht im Log).
///
bool RAD_ReportGameFinished(const char *command, const RAD_GameReport_t *report);

///
/// Holt Meldungen ab, die inzwischen fertig sind, ohne zu warten, und schreibt
/// je eine Zeile mit ihrem Ausgang ins Log. Liefert, wie viele es waren.
///
int32_t RAD_ReportCollectFinished(void);

#endif
