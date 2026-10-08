#ifndef __RAD_GAME_TURN_H__
#define __RAD_GAME_TURN_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/game_definitions.h>
#include <radish/game/model/model.h>
#include <radish/game/user.h>

///
/// Wer mitspielt, in welcher Reihenfolge und wer davon dran ist.
///
/// **Was in einem Zug erlaubt ist, steht nicht hier**, sondern an den
/// Einheiten: jede darf einmal ziehen und einmal angreifen
/// (RAD_Unit_t.turn, model/unit/unit.h). Einen Vorrat je Spieler gibt es
/// nicht.
///
/// **Die Reihe ist zugleich die Teilnehmerliste.** Beides getrennt zu fuehren
/// waere zweimal dieselbe Menge: wer mitspielt, kommt auch dran, und wer
/// drankommt, spielt mit. Es gab dafuer einmal eine eigene Liste daneben; sie
/// trug am Ende nichts als die Uuids, die ohnehin hier stehen, und war nur noch
/// synchron zu halten. Sobald an einem Mitspieler mehr haengt als seine Kennung
/// -- Farbe, Name, Punkte -- oder jemand mitspielen soll, ohne zu ziehen, kommt
/// sie zurueck, und dann mit einem Grund.
///
/// Es kennt weder Welt noch Spiel -- eine Reihenfolge ist eine Reihe von Uuids
/// und sonst nichts. Wer mitspielt, aendert sich damit ueber dieselben zwei
/// Funktionen wie die Reihenfolge; das Spiel reicht sie als RAD_GameAddPlayer und
/// RAD_GameRemovePlayer nach aussen (game.h).
///

///
/// Ergebnis einer Aenderung am Zug.
///
typedef enum
{
    RAD_TURN_OK = 0,

    /// RAD_USER_NONE ist kein Benutzer und kommt nicht in die Reihe.
    RAD_TURN_ERROR_NO_USER,

    /// Der Benutzer steht nicht in der Reihe und ist damit an keinem Zug.
    RAD_TURN_ERROR_NOT_IN_ORDER,

    /// Er steht in der Reihe, aber ein anderer ist dran.
    RAD_TURN_ERROR_NOT_YOUR_TURN,

    /// Kein Platz mehr in der Reihe -- RAD_MAX_PLAYERS.
    RAD_TURN_ERROR_FULL,

    /// Derselbe Benutzer steht zweimal in einer vorgegebenen Reihenfolge.
    RAD_TURN_ERROR_DUPLICATE
} RAD_TurnResult_t;

///
/// Macht daraus einen Text zum Loggen, wie RAD_ControlResultText. Immer ein
/// gueltiger Zeiger, auch bei einem Wert ausserhalb der Aufzaehlung.
///
const char* RAD_TurnResultText(RAD_TurnResult_t result);

///
/// Der Zug.
///
/// Die Reihenfolge steht als eigene Reihe da und folgt nicht aus einer Ablage.
/// Eine Reihenfolge, die aus der Ablage folgt, laesst sich nicht aendern, ohne
/// die Ablage umzuraeumen -- und ausgewuerfelt, nach Initiative oder rueckwaerts
/// sind alles Reihenfolgen, die ein Spiel haben will, ohne dass sich an den
/// Mitspielern etwas aendert.
///
struct RAD_Turn
{
    ///
    /// Die Mitspieler in der Reihenfolge, in der sie an die Reihe kommen: dicht
    /// ab 0, "number_of_users" Eintraege lang, dahinter RAD_USER_NONE. Anders als
    /// im Einheitenpool ist eine Luecke hier nicht zu gebrauchen -- der Index
    /// laeuft ueber die Reihe, und ein freier Platz waere ein Zug, der niemandem
    /// gehoert.
    ///
    /// RAD_MAX_PLAYERS lang, weil hier alle stehen, die mitspielen; die Grenze
    /// der Reihe ist damit die Grenze des Spiels.
    ///
    RAD_UserId_t order[RAD_MAX_PLAYERS];

    /// Belegte Eintraege in "order". 0 heisst, dass niemand dran ist.
    int32_t number_of_users;

    ///
    /// Wer dran ist -- als Stelle in "order" und nicht als Uuid: die Reihe ist
    /// die Wahrheit ueber den Zug, und zwei Angaben ueber denselben Zug koennten
    /// auseinanderlaufen. Wer die Uuid braucht, ruft RAD_TurnCurrentUser.
    ///
    /// Weitergeschaltet wird zyklisch; der Sprung von der letzten Stelle auf die
    /// 0 ist der Rundenwechsel. Eine fortlaufende Zugnummer gibt es damit nicht
    /// mehr -- einen Zugwechsel meldet das Spiel (RAD_EventManagerPublishTurnChanged).
    ///
    /// Gueltig ist [0, number_of_users); bei einer leeren Reihe steht sie auf 0
    /// und zeigt auf nichts.
    ///
    int32_t number;
};

///
/// Ein Zug ohne Reihenfolge, per Wert wie RAD_CreateWorld: eine Allokation
/// weniger, und die Ownership steht dort, wo er hingeschrieben wird. Niemand ist
/// dran.
///
RAD_Turn_t RAD_CreateTurn(void);

///
/// Haengt einen Benutzer hinten an die Reihe.
///
/// Hinten und nicht an der aktuellen Stelle: so verschiebt ein Beitritt den
/// laufenden Zug nicht. Wer dazukommt, ist in dieser Runde noch dran, wenn er
/// hinter dem steht, der gerade zieht, und sonst ab der naechsten.
///
/// Der erste eroeffnet zugleich den Zug -- seinen: sobald jemand in der Reihe
/// steht, ist auch jemand dran.
///
/// Zweimal derselbe ist RAD_TURN_OK und aendert nichts.
///
RAD_TurnResult_t RAD_TurnAddUser(RAD_Turn_t *turn, RAD_UserId_t user);

///
/// Nimmt einen Benutzer aus der Reihe; ein unbekannter ist kein Fehler, Gehen ist
/// idempotent.
///
/// Die Reihe bleibt dicht: alles hinter ihm rueckt eine Stelle vor. Wer dran war,
/// bleibt dran -- ausser er selbst war es, dann geht der Zug an den, der
/// nachgerueckt ist. War er der letzte in der Reihe, faengt sie wieder vorne
/// an.
///
void RAD_TurnRemoveUser(RAD_Turn_t *turn, RAD_UserId_t user);

///
/// Setzt die ganze Reihenfolge auf einmal -- der Grund, aus dem sie eine eigene
/// Reihe ist: ausgewuerfelt, nach Initiative, umgedreht.
///
/// **Sie setzt die Runde neu an**: danach ist der erste der Reihe dran. Wer sie mitten in einer Runde umstellt, faengt die Runde damit
/// von vorne an; gedacht ist sie fuer den Anfang eines Spiels oder einer Runde.
///
/// Geprueft wird alles, bevor irgendetwas geschrieben wird: ein abgelehnter
/// Aufruf laesst den Zug unveraendert. Abgelehnt wird eine Reihe, die laenger ist
/// als RAD_MAX_PLAYERS (RAD_TURN_ERROR_FULL), RAD_USER_NONE enthaelt
/// (RAD_TURN_ERROR_NO_USER) oder denselben Benutzer zweimal
/// (RAD_TURN_ERROR_DUPLICATE).
///
/// "number_of_users" darf 0 sein; "users" darf dann NULL sein. Danach ist niemand
/// dran.
///
RAD_TurnResult_t RAD_TurnSetOrder(RAD_Turn_t *turn, const RAD_UserId_t *users, int32_t number_of_users);

///
/// Die Reihe zum Nachlesen: erst zaehlen, dann einzeln holen. RAD_TurnUserAt
/// liefert RAD_USER_NONE fuer einen Index ausserhalb [0, RAD_TurnNumberOfUsers).
///
/// Anders als bei den Figuren eines Benutzers ist die Reihenfolge hier die
/// Aussage und keine Nebenwirkung der Ablage -- der Index 0 ist der, der die
/// Runde eroeffnet.
///
int32_t RAD_TurnNumberOfUsers(const RAD_Turn_t *turn);
RAD_UserId_t RAD_TurnUserAt(const RAD_Turn_t *turn, int32_t index);

///
/// Steht dieser Benutzer in der Reihe? Da die Reihe die Teilnehmerliste ist, ist
/// das zugleich die Frage, ob er mitspielt -- das Spiel reicht sie als
/// RAD_GameIsPlaying nach aussen.
///
/// RAD_USER_NONE steht nie darin, auch wenn die Plaetze hinter der Reihe diesen
/// Wert tragen.
///
bool RAD_TurnHasUser(const RAD_Turn_t *turn, RAD_UserId_t user);

///
/// Wer dran ist; RAD_USER_NONE, solange niemand in der Reihe steht.
///
RAD_UserId_t RAD_TurnCurrentUser(const RAD_Turn_t *turn);
bool RAD_TurnIsUsersTurn(const RAD_Turn_t *turn, RAD_UserId_t user);

///
/// Beendet den Zug und gibt ihn an den naechsten in der Reihe weiter. Hinter
/// dem letzten kommt wieder der erste -- das ist der Rundenwechsel.
///
/// Mit dem Benutzer als Argument, obwohl der Zug weiss, wer dran ist: nur so
/// laesst sich ein Kommando abweisen, das jemand schickt, der nicht an der Reihe
/// ist (RAD_TURN_ERROR_NOT_YOUR_TURN).
///
/// Steht nur einer in der Reihe, ist danach wieder er dran -- es ist ja ein
/// neuer Zug.
///
RAD_TurnResult_t RAD_TurnEnd(RAD_Turn_t *turn, RAD_UserId_t user);

#endif
