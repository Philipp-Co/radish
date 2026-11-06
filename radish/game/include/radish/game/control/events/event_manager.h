#ifndef __RAD_CONTROL_EVENT_MANAGER_H__
#define __RAD_CONTROL_EVENT_MANAGER_H__

#include <radish/game/model/model.h>
#include <radish/game/model/tile/tile.h>
#include <radish/game/model/unit/unit.h>
#include <radish/game/model/path/path.h>

///
/// control/events/ -- was aus dem Spiel herauskommt.
///
/// Es liegt unter control/, weil dort steht, was mit dem Spielzustand geschieht:
/// control/command/ ist der Weg hinein -- eine Absicht in Datenform --, und
/// dieses hier der Weg hinaus. Beides zusammen ist die Grenze des Spiels nach
/// aussen; was dazwischen liegt (world.h, turn.h), sind die Regeln selbst und
/// kennt weder Absender noch Abonnenten.
///
/// Ein Abonnent je Gruppe, und die Gruppe wird als Ganzes gesetzt. Die
/// Voreinstellung sind Callbacks, die nichts tun -- deshalb prueft keine
/// Publish-Funktion auf NULL, und deshalb muss ein Abonnent alle Zeiger seiner
/// Gruppe fuellen. Die Einheiten sind die Ausnahme in beidem: sie gibt es
/// zusaetzlich je Spieler, und fehlende Zeiger setzt der Manager selbst ein
/// (RAD_EventsUnitChangedCallback_t).
///
/// **Was oeffentlich ist, sind die Signaturen -- nicht der Manager.** Wer
/// abonniert, muss beschreiben koennen, was er abonniert; wie die Gruppen
/// gehalten und verteilt werden, geht ihn nichts an. RAD_EventManager_t ist
/// deshalb ein unvollstaendiger Typ, wie RAD_Game_t und RAD_Control_t: die
/// Struktur steht in event_manager.c und sonst nirgends.
///
/// Das kostet die Anlegefunktion ihren Rueckgabewert: einen Manager gibt es nur
/// noch als Zeiger aus RAD_CreateEventManager, nicht mehr als Wert auf dem
/// Stapel. Wer ihn haelt, gibt ihn mit RAD_DestroyEventManager wieder her.
///
/// **Warum oben tile.h, unit.h und path.h stehen.** Die Signaturen hier nennen
/// RAD_Tile_t, RAD_Unit_t und RAD_Path_t, und deren Namen stehen bei
/// ihren Strukturen und nicht in model.h (die Begruendung dort). Diese Datei
/// zieht damit alle drei Definitionen herein, und weil game.h sie einbindet,
/// bekommt sie jeder Konsument des Spielmoduls -- auch einer, der nie ein
/// Ereignis abonniert.
///
/// Das ist der bewusst gezahlte Preis dafuer, dass jeder dieser drei Namen genau
/// eine Quelle hat. Er ist klein, weil alle drei Strukturen klein sind und keine
/// weiteren Header nachziehen: tile.h kommt mit model.h aus, unit.h mit model.h
/// und user.h, path.h mit stdint allein. Ein zweiter typedef an dieser Stelle
/// waere der Ausweg gewesen -- in C99 ist er nicht erlaubt, und ein Name mit zwei
/// Quellen laeuft irgendwann auseinander.
///
/// **Der Pfad stand bis hierher in dieser Datei**, mit einem Define, das nach dem
/// Callback hiess, dem er gehoerte (RAD_ONUNITMOVED_MAX_STEPS). Seit ein
/// Kommando eine Bewegung als Pfad traegt, gehoert er nicht mehr einem allein: er
/// steht in model/path/path.h, und diese Datei nennt ihn nur noch.
///

typedef void (*RAD_OnTileAddedToGame_t)(void *user_argument, const RAD_Tile_t *tile);
typedef void (*RAD_OnTileRemovedFromGame_t)(void *user_argument, const RAD_Tile_t *tile);
typedef void (*RAD_OnTileStateChanged_t)(void *user_argument, const RAD_Tile_t *tile);

typedef struct 
{
    void *user_argument;
    RAD_OnTileAddedToGame_t added;
    RAD_OnTileRemovedFromGame_t removed;
    RAD_OnTileStateChanged_t changed;
} RAD_EventsTileChangedCallback_t;

typedef void (*RAD_OnUnitSpawned_t)(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y);
typedef void (*RAD_OnUnitDestroyed_t)(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y);

typedef void (*RAD_OnUnitMoved_t)(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result);

///
/// Die Sicht eines Spielers hat sich geaendert, ohne dass die Einheit selbst es
/// muss: "sighted" -- sie steht jetzt auf (x, y) in seiner Sicht und kommt ganz
/// mit, denn er kennt sie womoeglich noch nicht; "lost" -- sie ist aus seiner
/// Sicht verschwunden, (x, y) ist, wo er sie zuletzt sah. Gemeldet nur an die
/// Spieler, deren Sicht es betrifft (RAD_Recipients_t unten).
///
typedef void (*RAD_OnUnitSighted_t)(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y);
typedef void (*RAD_OnUnitLost_t)(void *user_argument, RAD_UnitId_t unit, int32_t x, int32_t y);

///
/// Hoechstzahl der Eintraege je Waffe in einem Angriff: jede Waffe jedes
/// Mitglieds der angreifenden Einheit hoechstens einmal.
///
#define RAD_ATTACK_MAX_WEAPONS (RAD_UNIT_MAX_MEMBERS * RAD_UNIT_MAX_WEAPONS)

///
/// Was eine Waffe in einem Angriff bewirkt hat: die Waffe "weapon" des
/// Mitglieds "member" der angreifenden Einheit, beides Indizes in ihrer
/// Reihenfolge beim Angriff. Die Zaehler wie in RAD_AttackResult_t; dazu, wie
/// viele Mitglieder des Ziels durch ihre Schuesse gefallen sind.
///
typedef struct
{
    int32_t member;
    int32_t weapon;
    int32_t shots;
    int32_t hits;
    int32_t penetrations;
    int32_t wounds;
    int32_t members_killed;
} RAD_AttackWeaponResult_t;

///
/// Was ein Angriff bewirkt hat (control/attack_unit.c). Die Zaehler sind die
/// Stufen eines Schusses, jede eine Teilmenge der vorigen: abgegeben, getroffen,
/// durch die Ruestung, gewirkt. Jeder wirkende Schuss kostet das getroffene
/// Mitglied einen Lebenspunkt, "wounds" sind also die abgezogenen Lebenspunkte.
///
/// "target" ist die Einheit, die auf (x, y) stand, als der Angriff begann --
/// RAD_UNIT_NONE fuer ein leeres Feld; dann wurde nicht geschossen und alle
/// Zaehler sind 0. "target_destroyed": sie hat dabei ihr letztes Mitglied
/// verloren und ist vom Feld (RAD_OnUnitDestroyed_t ist dann schon gemeldet).
///
/// "weapons" schluesselt dieselben Zaehler nach Waffen auf: ein Eintrag je
/// Waffe, die geschossen hat, in der Reihenfolge des Kampfes. Eine Waffe ausser
/// Reichweite fehlt, ebenso eine, die nach dem Fall des Ziels nicht mehr dran
/// kam. Die Eintraege zusammen ergeben die Zaehler oben. Fehlt der Schuetze in
/// einer Meldung (attacker RAD_UNIT_NONE), ist die Liste leer -- sie verriete,
/// wie er bewaffnet ist.
///
typedef struct
{
    RAD_UnitId_t attacker;
    RAD_UnitId_t target;
    int16_t x;
    int16_t y;
    int32_t shots;
    int32_t hits;
    int32_t penetrations;
    int32_t wounds;
    int32_t members_killed;
    bool target_destroyed;
    int32_t number_of_weapons;
    RAD_AttackWeaponResult_t weapons[RAD_ATTACK_MAX_WEAPONS];
} RAD_AttackResult_t;

///
/// Eine Einheit hat ein Feld angegriffen -- gemeldet nur fuer einen Angriff,
/// den das Spiel angenommen hat (RAD_GameCheckAttack), nachdem es ihn gezaehlt
/// (RAD_Unit_t.turn.attacked) und ausgewuerfelt hat.
///
typedef void (*RAD_OnUnitAttacked_t)(void *user_argument, const RAD_AttackResult_t *result);

typedef struct
{
    int16_t x;
    int16_t y; 
} RAD_Use_t;

///
/// Die Gruppe der Einheiten. "sighted" und "lost" kamen zuletzt dazu; wer sie
/// nicht setzt, laesst sie NULL, und der Manager setzt beim Abonnieren die
/// Voreinstellung ein, die nichts tut -- fuer diese Gruppe, und nur fuer sie,
/// muss ein Abonnent also nicht alle Zeiger fuellen.
///
typedef struct
{
    void *user_argument;
    RAD_OnUnitSpawned_t spawned;
    RAD_OnUnitDestroyed_t destroyed;
    RAD_OnUnitMoved_t moved;
    RAD_OnUnitAttacked_t attacked;
    RAD_OnUnitSighted_t sighted;
    RAD_OnUnitLost_t lost;
} RAD_EventsUnitChangedCallback_t;

///
/// Wer von einer Aenderung an Einheiten erfahren soll.
///
/// **Entschieden wird in control/**, dort, wo die Regeln stehen -- welcher
/// Spieler was sieht, weiss nur das Spiel. Das Modell reicht die Liste durch,
/// und der Manager stellt jedem Empfaenger zu, der fuer sich abonniert hat
/// (RAD_EventManagerSubscribeToUnitEventsOf). Der Manager selbst entscheidet
/// nichts; er kennt weder Spiel noch Sicht.
///
/// So viele Plaetze wie Mitspieler (RAD_MAX_PLAYERS, privat -- event_manager.c
/// prueft, dass beide gleich sind). Ein Spieler steht hoechstens einmal darin,
/// solange die Liste ueber RAD_RecipientsAdd waechst. NULL oder eine leere Liste
/// heisst: kein Spieler.
///
#define RAD_EVENT_MAX_RECIPIENTS 8

///
/// **"variant": eine weitere Fassung eines Ereignisses, das schon gemeldet ist.**
/// Bekommen nicht alle Empfaenger dasselbe -- ein Angriff ohne Schuetzen, ein
/// gekuerzter Weg --, meldet control/ einmal je Fassung. Die erste geht wie
/// jede Meldung auch an den Abonnenten ohne Empfaenger; jede weitere traegt
/// "variant" und geht nur an ihre Liste, damit er das Ereignis genau einmal
/// sieht. Leer initialisiert ist es false.
///
typedef struct
{
    RAD_UserId_t users[RAD_EVENT_MAX_RECIPIENTS];
    int32_t number_of_users;
    bool variant;
} RAD_Recipients_t;

///
/// Nimmt "user" in die Liste auf. true, wenn er danach darin steht -- auch, wenn
/// er es schon tat; false fuer RAD_USER_NONE, eine NULL-Liste oder eine volle.
///
bool RAD_RecipientsAdd(RAD_Recipients_t *recipients, RAD_UserId_t user);

///
/// Steht "user" in der Liste? false fuer NULL.
///
bool RAD_RecipientsContains(const RAD_Recipients_t *recipients, RAD_UserId_t user);

///
/// Der Zugwechsel: ein neuer Zug beginnt, oder niemand ist mehr dran.
/// "current_user" ist, wer jetzt dran ist, RAD_USER_NONE fuer niemand.
///
/// Gemeldet wird er vom Spiel (player.c), nicht vom Zug (turn.h) -- der ist Regel
/// und kennt keine Abonnenten. Jedes erfolgreiche Abgeben meldet ihn, auch wenn
/// derselbe wieder dran ist: es ist ein neuer Zug. Beitritt und Austritt melden
/// ihn nur, wenn sich dadurch aendert, wer dran ist.
///
typedef void (*RAD_OnTurnChanged_t)(void *user_argument, RAD_UserId_t current_user);

typedef struct
{
    void *user_argument;
    RAD_OnTurnChanged_t changed;
} RAD_EventsTurnCallback_t;

///
/// Warum ein Spiel vorbei ist (RAD_GameEndReason).
///
///   NONE        es laeuft noch
///   ROUNDS      die letzte Runde ist um, gewertet wurde nach Punkten
///               (RAD_GameScore) -- bei Gleichstand ohne Sieger
///   ELIMINATED  ein Spieler hat weder in der Reserve noch auf dem Feld eine
///               Einheit mehr; er verliert
///
typedef enum
{
    RAD_GAME_END_NONE = 0,
    RAD_GAME_END_ROUNDS,
    RAD_GAME_END_ELIMINATED
} RAD_GameEndReason_t;

///
/// Das Spiel ist vorbei: "winner" hat gewonnen, RAD_USER_NONE bei einem
/// Unentschieden. Gemeldet genau einmal, vom Spiel (control/victory.c); danach
/// nimmt es kein Kommando mehr an.
///
typedef void (*RAD_OnGameFinished_t)(void *user_argument, RAD_UserId_t winner, RAD_GameEndReason_t reason);

typedef struct
{
    void *user_argument;
    RAD_OnGameFinished_t finished;
} RAD_EventsGameCallback_t;

///
/// Die Zeigereignisse: was der Benutzer mit der Maus tut, nicht was im Spiel
/// geschieht. Sie melden keine Aenderung am Zustand, und deshalb veroeffentlicht
/// sie niemand aus dem Modell heraus -- der Client tut es aus seiner Schleife, und
/// das Rendering hoert zu.
///
/// Sie stehen trotzdem hier, weil der Manager die einzige Stelle ist, an der ein
/// Erzeuger und ein Abonnent sich finden, ohne einander zu kennen. Beim Umzug
/// dieser Datei von events/ nach control/events/ waren sie eine Weile nur noch in
/// event_manager.c erklaert; ein Name mit keiner Quelle ist schlimmer als einer
/// mit zwei.
///
typedef void (*RAD_OnMouseMove_t)(void *user_argument, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y);
typedef void (*RAD_OnMousePressed_t)(void *user_argument, int32_t x, int32_t y);
typedef void (*RAD_OnMouseReleased_t)(void *user_argument, int32_t x, int32_t y);

typedef struct 
{
    void *user_argument;
    RAD_OnMouseMove_t move;
    RAD_OnMousePressed_t pressed;
    RAD_OnMouseReleased_t released;
} RAD_EventsMouseCallbacks_t;

///
/// Der Manager selbst: nur ein Name. Was er haelt -- je eine Gruppe von oben --,
/// steht in event_manager.c.
///
typedef struct RAD_EventManager RAD_EventManager_t;

///
/// Legt einen Manager an, mit Callbacks, die nichts tun; NULL, wenn kein Speicher
/// da ist. Der einzige Weg an einen: die Struktur ist unvollstaendig, ein
/// Aufrufer kann sie weder anlegen noch ihre Groesse erfragen.
///
/// RAD_DestroyEventManager nullt den Zeiger des Aufrufers, wie RAD_DestroyGame.
/// Er muss laenger leben als alles, was ihn hinterlegt hat -- ein Spiel haelt ihn
/// geliehen und baut ihn nicht ab.
///
RAD_EventManager_t* RAD_CreateEventManager(void);
void RAD_DestroyEventManager(RAD_EventManager_t **manager);

void RAD_EventManagerSubscribeToTileEvents(RAD_EventManager_t *manager, RAD_EventsTileChangedCallback_t callbacks);
void RAD_EventManagerPublishTileAddedToGameEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile);
void RAD_EventManagerPublishTileRemovedFromGameEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile);
void RAD_EventManagerPublishTileStateChangeEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile);

///
/// Die Einheiten gibt es auf zwei Wegen zu abonnieren.
///
/// **Ohne Empfaenger** (RAD_EventManagerSubscribeToUnitEvents): ein Abonnent,
/// der jedes Ereignis genau einmal bekommt, ungefiltert -- die Sicht des
/// Schiedsrichters, fuer Log und Tests. Wer Spielern etwas schicken will, ist
/// hier falsch: er saehe, was sie nicht sehen duerfen.
///
/// **Je Spieler** (RAD_EventManagerSubscribeToUnitEventsOf): ein Abonnent je
/// Benutzer, der nur bekommt, wofuer ihn eine Liste nennt. Ein zweites Abo fuer
/// denselben Benutzer ersetzt das erste. false, wenn "user" RAD_USER_NONE ist
/// oder schon RAD_EVENT_MAX_RECIPIENTS andere abonniert haben; dann aendert
/// sich nichts. RAD_EventManagerUnsubscribeUser nimmt ihn wieder heraus, ein
/// Benutzer ohne Abo ist kein Fehler.
///
/// Jede Publish-Funktion ruft zuerst den Abonnenten ohne Empfaenger -- nicht
/// bei einer weiteren Fassung (RAD_Recipients_t.variant) --, dann je Eintrag in
/// "recipients" den Abonnenten dieses Benutzers, sofern es ihn gibt -- in der
/// Reihenfolge der Liste.
///
void RAD_EventManagerSubscribeToUnitEvents(RAD_EventManager_t *manager, RAD_EventsUnitChangedCallback_t callbacks);
bool RAD_EventManagerSubscribeToUnitEventsOf(RAD_EventManager_t *manager, RAD_UserId_t user, RAD_EventsUnitChangedCallback_t callbacks);
void RAD_EventManagerUnsubscribeUser(RAD_EventManager_t *manager, RAD_UserId_t user);
void RAD_EventManagerPublishUnitSpawned(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                        const RAD_Unit_t *unit, int32_t x, int32_t y);
void RAD_EventManagerPublishUnitDestroyed(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                          const RAD_Unit_t *unit, int32_t x, int32_t y);
void RAD_EventManagerPublishUnitMoved(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                      const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result);
void RAD_EventManagerPublishUnitAttacked(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                         const RAD_AttackResult_t *result);
void RAD_EventManagerPublishUnitSighted(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                        const RAD_Unit_t *unit, int32_t x, int32_t y);
void RAD_EventManagerPublishUnitLost(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                     RAD_UnitId_t unit, int32_t x, int32_t y);

void RAD_EventManagerSubscribeToTurnEvents(RAD_EventManager_t *manager, RAD_EventsTurnCallback_t callbacks);
void RAD_EventManagerPublishTurnChanged(RAD_EventManager_t *manager, RAD_UserId_t current_user);

void RAD_EventManagerSubscribeToGameEvents(RAD_EventManager_t *manager, RAD_EventsGameCallback_t callbacks);
void RAD_EventManagerPublishGameFinished(RAD_EventManager_t *manager, RAD_UserId_t winner, RAD_GameEndReason_t reason);

void RAD_EventManagerSubscribeToMouseEvents(RAD_EventManager_t *manager, RAD_EventsMouseCallbacks_t callbacks);
void RAD_EventManagerPublishMouseMoved(RAD_EventManager_t *manager, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y);
void RAD_EventManagerPublishMousePressed(RAD_EventManager_t *manager, int32_t x, int32_t y);
void RAD_EventManagerPublishMouseReleased(RAD_EventManager_t *manager, int32_t x, int32_t y);

#endif
