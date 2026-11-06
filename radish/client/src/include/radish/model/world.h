#ifndef __RAD_MODEL_WORLD_H__
#define __RAD_MODEL_WORLD_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <radish/model/events/observable.h>
#include <radish/model/reserve.h>
#include <radish/model/tile.h>
#include <radish/model/unit.h>
#include <radish/model/unit_repository.h>
#include <radish/rendering/iso_definitions.h>

///
/// Was der Client von der Welt weiss -- und nur das.
///
/// **Ein Abbild, keine Simulation.** Gefuellt wird es aus dem, was der Server
/// schickt: den Feldern einer Discover-Antwort, den Tile-Ereignissen, den
/// Einheiten der Reserve und den Zuegen, die er bestaetigt hat. Regeln wendet es
/// keine an; ob ein Zug geht, entscheidet der Server. Die Views lesen hier
/// nach, was auf einem Feld liegt.
///
/// **Unabhaengig von der Verbindung**, wie alles im Modell: die Welt kennt nur
/// eigene Typen. Was der Server schickt, uebersetzen die Event-Handler
/// (io/net_event_handler/), bevor es hier ankommt.
///
/// Das Raster ist so gross wie das der Darstellung (RAD_ISO_MAP_SIZE). Ein Feld
/// ausserhalb davon wird verworfen, genau wie in der Iso-Map.
///
/// **Alle Einheiten stehen im Unit-Repository der Welt** ("units",
/// model/unit_repository.h) -- die in einer Reserve wie die auf dem Feld. Eine
/// Reserve nennt nur Ids, RAD_ClientTile_t.unit zeigt in das Repository. Beim
/// Deployen wechselt eine Einheit nur, wer sie nennt; sie selbst bleibt an
/// ihrem Platz.
///
/// **Auf einem Feld steht eine Einheit nur, wenn die Welt sie schon kennt**:
/// aus einem Deployment (RAD_ClientWorldDeployUnit) oder aus der Antwort auf
/// eine Einheiten-Anfrage (RAD_ClientWorldAddUnit), und in keiner Reserve.
/// Nennt der Server auf einem Feld eine Einheit, die sie (noch) nicht kennt,
/// bleibt das Feld ohne -- bis es erneut kommt. Der Client fragt deshalb nach
/// den Einheiten, bevor er nach den Feldern fragt (main.c).
///
/// **Beobachtbar** (model/events/observable.h), wie alles im Model: die Welt
/// meldet, was in ihr neu dazukommt -- "tile_created", wenn der Server ein Feld
/// zum ersten Mal schickt, "reserve_created", wenn ein Besitzer seine Reserve
/// bekommt -- und "size_changed". Was sich an einem Feld oder einer Reserve
/// aendert, melden die selbst; wer das wissen will, meldet sich dort an.
///
/// **Die Welt gibt Zeiger heraus, keine Kopien** (RAD_ClientWorldTileAt,
/// RAD_ClientWorldReserve, RAD_ClientWorldUnits): an einer Kopie liesse sich
/// niemand anmelden, der etwas erfahren soll.
///

///
/// Ein Feld im Raster der Welt, in Weltkoordinaten.
///
typedef struct
{
    int32_t x;
    int32_t y;
} RAD_ClientPosition_t;

///
/// Hoechstzahl der Eintraege je Waffe in einem Angriff: jede Waffe jedes
/// Mitglieds hoechstens einmal.
///
#define RAD_CLIENT_ATTACK_WEAPONS_MAX (RAD_CLIENT_UNIT_MEMBERS_MAX * RAD_CLIENT_MEMBER_WEAPONS_MAX)

///
/// Was eine Waffe in einem Angriff bewirkt hat: die Waffe "weapon" des
/// Mitglieds "member" der angreifenden Einheit, als Indizes in
/// RAD_ClientUnit_t.members und RAD_ClientMember_t.weapons. Die Zaehler wie in
/// RAD_ClientAttackResult_t.
///
typedef struct
{
    uint32_t member;
    uint32_t weapon;
    uint32_t shots;
    uint32_t hits;
    uint32_t penetrations;
    uint32_t wounds;
    uint32_t members_killed;
} RAD_ClientAttackWeapon_t;

///
/// Ein Angriff, wie der Server ihn bestaetigt hat: die Einheit "attacker" hat
/// das Feld "target" angegriffen -- RAD_CLIENT_UNIT_ID_NONE, wenn der Server
/// den Schuetzen nicht nennt.
///
/// "victim" ist die Einheit, die auf dem Feld stand, RAD_CLIENT_UNIT_ID_NONE
/// fuer ein leeres Feld. Die Zaehler sind die Stufen ihrer Schuesse, jede eine
/// Teilmenge der vorigen: abgegeben, getroffen, durch die Ruestung, gewirkt
/// (= abgezogene Lebenspunkte); dazu die gefallenen Mitglieder und ob das Ziel
/// zerstoert ist -- dann hat die Welt es schon nicht mehr.
///
/// "weapons" schluesselt die Zaehler nach Waffen auf, ein Eintrag je Waffe, die
/// geschossen hat. Leer, wenn der Schuetze unbekannt ist oder der Server die
/// Liste nicht mitschicken konnte; die Zaehler oben gelten immer.
///
typedef struct
{
    RAD_ClientUnitId_t attacker;
    RAD_ClientPosition_t target;
    RAD_ClientUnitId_t victim;
    uint32_t shots;
    uint32_t hits;
    uint32_t penetrations;
    uint32_t wounds;
    uint32_t members_killed;
    bool victim_destroyed;
    uint32_t number_of_weapons;
    RAD_ClientAttackWeapon_t weapons[RAD_CLIENT_ATTACK_WEAPONS_MAX];
} RAD_ClientAttackResult_t;

///
/// Spieler einer Partie, und damit Reserven in der Welt.
///
#define RAD_CLIENT_WORLD_PLAYERS 2

///
/// Wo "owner" Einheiten aus seiner Reserve aufstellen darf: das Rechteck von
/// (min_x, min_y) bis (max_x, max_y), beide Ecken eingeschlossen, in
/// Weltkoordinaten. So, wie der Server es schickt (game.proto
/// NetDeployZoneEvent) -- der Client kennt die Regel dahinter nicht.
/// RAD_CLIENT_PLAYER_NONE als Besitzer: ein freier Platz.
///
typedef struct
{
    RAD_ClientPlayerId_t owner;
    int32_t min_x;
    int32_t min_y;
    int32_t max_x;
    int32_t max_y;
} RAD_ClientDeployZone_t;

///
/// Wie ein Spiel ausgegangen ist, so wie der Server es meldet
/// (NetGameFinishedEvent): ob es vorbei ist, wer gewonnen hat --
/// RAD_CLIENT_PLAYER_NONE bei einem Unentschieden --, ob nach Punkten oder weil
/// einer keine Einheit mehr hatte, und die Punkte je Mitspieler, "number_of_players"
/// belegt.
///
typedef struct
{
    bool finished;
    RAD_ClientPlayerId_t winner;
    bool eliminated;
    RAD_ClientPlayerId_t players[RAD_CLIENT_WORLD_PLAYERS];
    int32_t scores[RAD_CLIENT_WORLD_PLAYERS];
    int32_t number_of_players;
} RAD_ClientGameResult_t;

typedef struct
{
    RAD_ClientTile_t tiles[RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];

    ///
    /// Wer mitspielt, in der Reihenfolge ihrer Zuege, wie der Server sie
    /// schickt (game.proto NetPlayersEvent); "number_of_players" belegt. Nur
    /// ueber RAD_ClientWorldSetPlayers. Die Stelle eines Spielers darin gibt
    /// seine Farbe vor (RAD_ClientWorldPlayerIndex) -- auf beiden Clients
    /// dieselbe.
    ///
    RAD_ClientPlayerId_t players[RAD_CLIENT_WORLD_PLAYERS];
    int32_t number_of_players;

    ///
    /// Ob der Server dieses Feld schon geschickt hat. Ein unbekanntes Feld ist
    /// keines -- RAD_ClientWorldTileAt liefert dafuer NULL.
    ///
    bool known[RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];

    uint32_t width;
    uint32_t height;

    ///
    /// Die Reserven der beiden Spieler, in der Reihenfolge, in der ihre Besitzer
    /// zuerst vom Server kommen (RAD_ClientWorldAddReserveUnit).
    ///
    RAD_ClientReserve_t reserves[RAD_CLIENT_WORLD_PLAYERS];

    ///
    /// Alle Einheiten der Welt, in einer Reserve oder auf dem Feld (siehe oben).
    ///
    RAD_ClientUnitRepository_t units;

    ///
    /// Wer gerade am Zug ist, wie der Server es zuletzt geschickt hat;
    /// RAD_CLIENT_PLAYER_NONE fuer niemand oder solange er es nicht geschickt
    /// hat. Nur ueber RAD_ClientWorldSetCurrentPlayer.
    ///
    RAD_ClientPlayerId_t current_player;

    ///
    /// Die Aufstellzonen der beiden Spieler, in der Reihenfolge, in der der
    /// Server sie zuerst schickt (RAD_ClientWorldSetDeployZone). Wie
    /// "current_player" ohne Meldung: wer sie zeigt, liest sie beim Zeichnen.
    ///
    RAD_ClientDeployZone_t deploy_zones[RAD_CLIENT_WORLD_PLAYERS];

    ///
    /// Die laufende Runde und wie viele es gibt, wie der Server sie zuletzt
    /// geschickt hat; beide 0, solange er es nicht getan hat. Und wie das Spiel
    /// ausgegangen ist. Beides wie "current_player" ohne Meldung: wer es zeigt,
    /// liest es beim Zeichnen (view/end_turn_view.h).
    ///
    int32_t round;
    int32_t rounds;
    RAD_ClientGameResult_t result;

    /// Wer die Welt beobachtet -- nur ueber RAD_ClientWorldSubscribe.
    RAD_ModelObservable_t observable;
} RAD_ClientWorld_t;

typedef enum
{
    RAD_CLIENT_WORLD_EVENT_TILE_CREATED = 0,
    RAD_CLIENT_WORLD_EVENT_RESERVE_CREATED,
    RAD_CLIENT_WORLD_EVENT_SIZE_CHANGED,
    RAD_CLIENT_WORLD_EVENT_UNIT_MOVED,
    RAD_CLIENT_WORLD_EVENT_UNIT_ATTACKED,
    RAD_CLIENT_WORLD_EVENT_ATTACK_REJECTED,
    RAD_CLIENT_WORLD_EVENT_PLAYERS_CHANGED
} RAD_ClientWorldEvent_t;

///
/// Ein Beobachter der Welt. "world" ist die Welt selbst; "tile" und "reserve"
/// stehen in ihr und gelten, solange sie nicht neu initialisiert wird. Ein
/// NULL-Callback heisst: dieses Ereignis nicht.
///
/// In "tile_created" und "reserve_created" kann sich der Beobachter gleich am
/// neuen Objekt anmelden -- es hat bis dahin noch nichts gemeldet.
///
/// "unit_moved" meldet einen Zug samt Weg (RAD_ClientWorldMoveEntity), **bevor**
/// die Welt ihn ausfuehrt: die Einheit steht dabei noch auf steps[0]. "steps"
/// gehoert dem Aufrufer und gilt nur waehrend des Callbacks.
///
/// "unit_attacked" meldet einen Angriff samt dem, was er bewirkt hat
/// (RAD_ClientWorldReportAttack). Die Welt aendert dabei nichts. "result"
/// gehoert dem Aufrufer und gilt nur waehrend des Callbacks.
///
/// "attack_rejected" meldet, dass der Server einen eigenen Angriff der Einheit
/// "unit" abgelehnt hat (RAD_ClientWorldReportAttackRejected). Die Welt aendert
/// dabei ebenso nichts.
///
/// "players_changed" meldet, dass sich die Mitspieler oder ihre Reihenfolge
/// geaendert haben (RAD_ClientWorldSetPlayers) -- und damit womoeglich die Farbe
/// der Einheiten auf dem Feld.
///
typedef struct
{
    void *user_argument;
    void (*tile_created)(void *user_argument, const RAD_ClientWorld_t *world, const RAD_ClientTile_t *tile);
    void (*reserve_created)(void *user_argument, const RAD_ClientWorld_t *world, const RAD_ClientReserve_t *reserve);
    void (*size_changed)(void *user_argument, const RAD_ClientWorld_t *world);
    void (*unit_moved)(void *user_argument, const RAD_ClientWorld_t *world, RAD_ClientUnitId_t unit,
                       const RAD_ClientPosition_t *steps, size_t count);
    void (*unit_attacked)(void *user_argument, const RAD_ClientWorld_t *world,
                          const RAD_ClientAttackResult_t *result);
    void (*attack_rejected)(void *user_argument, const RAD_ClientWorld_t *world, RAD_ClientUnitId_t unit);
    void (*players_changed)(void *user_argument, const RAD_ClientWorld_t *world);
} RAD_ClientWorldObserver_t;

///
/// Bringt "world" in den Grundzustand: kein Feld bekannt, Groesse null, beide
/// Reserven frei und leer, keine Einheiten, niemand am Zug, **ohne Beobachter** -- weder an
/// der Welt noch an irgendetwas in ihr. Zeiger in die Welt zeigen danach auf leere Objekte.
///
void RAD_ClientWorldInit(RAD_ClientWorld_t *world);

///
/// Setzt die Groesse und meldet "size_changed" -- nur, wenn sie sich aendert.
///
void RAD_ClientWorldSetSize(RAD_ClientWorld_t *world, uint32_t width, uint32_t height);

///
/// Uebernimmt das Feld "position" vom Typ "type", wie es der Server schickt.
/// Ein Feld, das die Welt noch nicht kannte, meldet sie als "tile_created";
/// eines, das sie schon kannte, meldet selbst "changed" (RAD_ClientTileSet).
///
/// "unit" ist die Einheit, die der Server auf dem Feld nennt,
/// RAD_CLIENT_UNIT_ID_NONE fuer keine. Ist sie auf dem Feld bekannt, steht sie
/// darauf; sonst bleibt es ohne (siehe oben). "victory": ob es ein Siegfeld ist,
/// "terrain": sein Gelaende.
///
void RAD_ClientWorldApplyTile(RAD_ClientWorld_t *world,
                              RAD_ClientPosition_t position,
                              RAD_ClientTileType_t type,
                              RAD_ClientUnitId_t unit,
                              bool victory,
                              RAD_ClientTerrain_t terrain);

///
/// Verwirft das Feld (x, y): es meldet "removed", seine Beobachter sind danach
/// abgemeldet (RAD_ClientTileRemove). Ein unbekanntes Feld bleibt still.
///
void RAD_ClientWorldRemoveTile(RAD_ClientWorld_t *world, uint32_t x, uint32_t y);

///
/// Das Feld (x, y), oder NULL, wenn es ausserhalb liegt oder nicht bekannt ist.
/// Der Zeiger gilt, solange die Welt nicht neu initialisiert wird; verwirft sie
/// das Feld, zeigt er auf ein unbekanntes -- wer das wissen muss, beobachtet es.
///
const RAD_ClientTile_t* RAD_ClientWorldTileAt(const RAD_ClientWorld_t *world, int32_t x, int32_t y);

///
/// Setzt die Einheit "unit" den Weg "steps" entlang: von steps[0], ihrem
/// Standort, auf steps[count - 1], das Ziel. Fuer einen Zug, den der Server
/// bestaetigt hat -- er schickt die Aenderung der Felder bislang nicht als
/// Tile-Ereignis mit, also traegt der Client sie selbst nach.
///
/// **Gemeldet wird zuerst "unit_moved"** mit dem ganzen Weg, die Einheit steht
/// da noch auf steps[0]. Danach melden Start- und Zielfeld "changed"
/// (RAD_ClientTileSetUnit). Die Felder dazwischen prueft die Welt nicht, sie
/// reicht sie nur weiter.
///
/// **Der Weg eines Beobachters** (Nebel des Krieges) ist nur der Teil, den er
/// sah, und faengt womoeglich dort an, wo die Einheit gerade erst in seine Sicht
/// kam. Steht auf steps[0] keine Einheit, kennt die Welt sie aber und steht sie
/// auf keinem Feld -- sie war schon einmal zu sehen --, wird sie dort hingestellt
/// und zieht von da. Steht sie noch auf einem anderen Feld, wird sie von dort
/// genommen. Felder dazwischen koennen fehlen; der Weg springt dann.
///
/// Unter zwei Feldern, mit Start oder Ziel ausserhalb der Welt, oder wenn die
/// Einheit weder auf steps[0] steht noch dorthin gestellt werden kann, geschieht
/// nichts, und es wird nichts gemeldet.
///
void RAD_ClientWorldMoveEntity(RAD_ClientWorld_t *world, RAD_ClientUnitId_t unit,
                               const RAD_ClientPosition_t *steps, size_t count);

///
/// Meldet "unit_attacked": ein Angriff, wie der Server ihn bestaetigt hat, mit
/// dem, was er bewirkt hat ("result"). Nur fuer die Beobachter -- in die Welt
/// tragen ihn die neuen Staende der Einheiten ein
/// (io/net_event_handler/unit_attacked.c). Ein Ziel ausserhalb der Welt meldet
/// sie nicht.
///
void RAD_ClientWorldReportAttack(const RAD_ClientWorld_t *world, const RAD_ClientAttackResult_t *result);

///
/// Meldet "attack_rejected": der Server hat einen eigenen Angriff der Einheit
/// "unit" abgelehnt. Nur fuer die Beobachter -- etwa, damit ein Turm, der schon
/// auf dem Ziel steht, zurueckdreht (view/iso_map_view.h).
///
void RAD_ClientWorldReportAttackRejected(const RAD_ClientWorld_t *world, RAD_ClientUnitId_t unit);

///
/// Uebernimmt "unit" in das Repository und ihre Id in die Reserve ihres
/// Besitzers (unit->owner). Hat noch keine Reserve diesen Besitzer, bekommt er
/// die erste freie, und die Welt meldet "reserve_created" -- vor dem
/// "unit_added" der Reserve.
///
/// **Eine Einheit, die die Welt schon kennt** (gleiche Id), wird an ihrem Platz
/// ersetzt (RAD_ClientUnitAssign, sie meldet "changed") -- der Server schickt
/// die Reserve auf jede Anfrage ganz. Steht ihre Id schon in der Reserve, meldet
/// die Reserve nichts.
///
/// false, wenn der Besitzer RAD_CLIENT_PLAYER_NONE ist, schon beide Reserven anderen
/// gehoeren, seine Reserve voll ist oder das Repository -- dann bleibt alles,
/// wie es war.
///
bool RAD_ClientWorldAddReserveUnit(RAD_ClientWorld_t *world, const RAD_ClientUnit_t *unit);

///
/// Uebernimmt "unit" in das Repository -- nur dorthin, in keine Reserve und auf
/// kein Feld. Fuer eine Einheit aus einer Einheiten-Anfrage, die nicht sagt, wo
/// sie steht (io/net_event_handler/unit.c).
///
/// **Eine Einheit, die die Welt schon kennt**, wird an ihrem Platz ersetzt
/// (RAD_ClientUnitAssign, sie meldet "changed"); wo sie steht, bleibt.
///
/// false, wenn der Besitzer RAD_CLIENT_PLAYER_NONE ist oder im Repository kein
/// Platz mehr frei ist -- dann bleibt alles, wie es war.
///
bool RAD_ClientWorldAddUnit(RAD_ClientWorld_t *world, const RAD_ClientUnit_t *unit);

///
/// Nimmt die Einheit mit der Id "id" aus der Reserve, in der sie steht --
/// gesucht wird in beiden --, und aus der Welt: die Reserve meldet
/// "unit_removed", danach die Einheit "removed" (RAD_ClientUnitRepositoryRemove).
/// Die Reserve behaelt ihren Besitzer, auch wenn sie danach leer ist.
///
/// false, wenn keine Reserve eine Einheit mit dieser Id hat.
///
bool RAD_ClientWorldRemoveReserveUnit(RAD_ClientWorld_t *world, RAD_ClientUnitId_t id);

///
/// Nimmt die Einheit "id" ganz aus der Welt -- fuer eine, die der Server als
/// zerstoert gemeldet hat (io/net_event_handler/unit_destroyed.c). Steht sie auf
/// "position", wird das Feld frei und meldet "changed" (RAD_ClientTileSetUnit);
/// steht sie in einer Reserve, meldet die "unit_removed". Danach meldet die
/// Einheit "removed" (RAD_ClientUnitRepositoryRemove).
///
/// false, wenn die Welt die Einheit nicht kennt -- dann bleibt alles, wie es war.
///
bool RAD_ClientWorldRemoveUnit(RAD_ClientWorld_t *world, RAD_ClientUnitId_t id, RAD_ClientPosition_t position);

///
/// Stellt die Einheit "id" aus der Reserve, in der sie steht -- gesucht wird in
/// beiden --, auf das Feld "position". Fuer ein Deployment, das der Server
/// gemeldet hat (io/net_event_handler/unit_deployed.c).
///
/// Die Einheit bleibt, wo sie im Repository steht, mit ihren Beobachtern; die
/// Reserve meldet "unit_removed", das Feld "changed" (RAD_ClientTileSetUnit).
///
/// false, wenn das Feld nicht bekannt ist oder schon eine Einheit traegt oder
/// keine Reserve die Einheit hat -- dann bleibt alles, wie es war.
///
bool RAD_ClientWorldDeployUnit(RAD_ClientWorld_t *world, RAD_ClientUnitId_t id, RAD_ClientPosition_t position);

///
/// Eine fremde Einheit ist in Sicht gekommen (io/net_event_handler/
/// unit_sighted.c): sie kommt in das Repository -- eine bekannte wird ersetzt
/// --, aus jeder Reserve heraus, von jedem anderen Feld herunter und auf das
/// Feld "position". Die Felder melden "changed".
///
/// false, wenn der Besitzer RAD_CLIENT_PLAYER_NONE ist, das Repository voll,
/// das Feld unbekannt oder von einer anderen Einheit besetzt -- im letzten Fall
/// steht sie danach im Repository, aber auf keinem Feld.
///
bool RAD_ClientWorldShowUnit(RAD_ClientWorld_t *world, const RAD_ClientUnit_t *unit, RAD_ClientPosition_t position);

///
/// Eine fremde Einheit ist aus der Sicht verschwunden (io/net_event_handler/
/// unit_lost.c): sie kommt vom Feld, auf dem sie steht -- meist "position", der
/// Stelle, an der sie zuletzt zu sehen war --, bleibt aber im Repository. Sie ist
/// nicht zerstoert; kommt sie wieder in Sicht, ist sie bekannt. Das Feld meldet
/// "changed".
///
/// false, wenn die Welt sie nicht kennt oder sie auf keinem Feld steht.
///
bool RAD_ClientWorldHideUnit(RAD_ClientWorld_t *world, RAD_ClientUnitId_t id, RAD_ClientPosition_t position);

///
/// Die Reserve von "owner", oder NULL, wenn keine ihm gehoert. Der Zeiger gilt,
/// solange die Welt nicht neu initialisiert wird.
///
const RAD_ClientReserve_t* RAD_ClientWorldReserve(const RAD_ClientWorld_t *world, RAD_ClientPlayerId_t owner);

///
/// Wer am Zug ist (siehe oben). Gesetzt wird es aus dem Ereignis des Servers
/// (io/net_event_handler/current_player.c). **Gemeldet wird es nicht**: wer es
/// anzeigt, liest es bei jedem Frame (view/end_turn_view.h).
///
void RAD_ClientWorldSetCurrentPlayer(RAD_ClientWorld_t *world, RAD_ClientPlayerId_t player);
RAD_ClientPlayerId_t RAD_ClientWorldCurrentPlayer(const RAD_ClientWorld_t *world);

///
/// Uebernimmt die Mitspieler "players" in der Reihenfolge ihrer Zuege, wie der
/// Server sie schickt (io/net_event_handler/players.c). Mehr als
/// RAD_CLIENT_WORLD_PLAYERS faellt weg, ebenso RAD_CLIENT_PLAYER_NONE. Meldet
/// "players_changed" -- nur, wenn sich etwas aendert.
///
void RAD_ClientWorldSetPlayers(RAD_ClientWorld_t *world, const RAD_ClientPlayerId_t *players, size_t count);

///
/// Die Stelle von "player" unter den Mitspielern (0 fuer den, der zuerst zieht),
/// oder -1, wenn er nicht mitspielt oder RAD_CLIENT_PLAYER_NONE ist.
///
int32_t RAD_ClientWorldPlayerIndex(const RAD_ClientWorld_t *world, RAD_ClientPlayerId_t player);

///
/// Runde und Ausgang des Spiels (siehe oben). Gesetzt aus den Ereignissen des
/// Servers (io/net_event_handler/round.c, game_finished.c), **ohne Meldung**.
/// Ein Ausgang gilt, bis die Welt neu initialisiert wird; der Zeiger ebenso.
///
void RAD_ClientWorldSetRound(RAD_ClientWorld_t *world, int32_t round, int32_t rounds);
int32_t RAD_ClientWorldRound(const RAD_ClientWorld_t *world);
int32_t RAD_ClientWorldNumberOfRounds(const RAD_ClientWorld_t *world);
void RAD_ClientWorldSetResult(RAD_ClientWorld_t *world, const RAD_ClientGameResult_t *result);
const RAD_ClientGameResult_t* RAD_ClientWorldResult(const RAD_ClientWorld_t *world);
bool RAD_ClientWorldIsFinished(const RAD_ClientWorld_t *world);

///
/// Uebernimmt die Aufstellzone "zone" fuer zone->owner, wie der Server sie
/// schickt (io/net_event_handler/deploy_zone.c): eine schon bekannte wird
/// ersetzt, sonst nimmt sie einen freien Platz. false fuer
/// RAD_CLIENT_PLAYER_NONE, ein leeres Rechteck oder wenn beide Plaetze schon
/// anderen gehoeren. **Gemeldet wird es nicht**, wie beim Zug.
///
bool RAD_ClientWorldSetDeployZone(RAD_ClientWorld_t *world, const RAD_ClientDeployZone_t *zone);

///
/// Die Aufstellzone von "owner", oder NULL, solange der Server keine geschickt
/// hat. Der Zeiger gilt, solange die Welt nicht neu initialisiert wird.
///
const RAD_ClientDeployZone_t* RAD_ClientWorldDeployZone(const RAD_ClientWorld_t *world, RAD_ClientPlayerId_t owner);

///
/// Liegt (x, y) in der Aufstellzone von "owner"? false, wenn er keine hat.
///
bool RAD_ClientWorldInDeployZone(const RAD_ClientWorld_t *world, RAD_ClientPlayerId_t owner, int32_t x, int32_t y);

///
/// Alle Einheiten der Welt, zum Nachschlagen einer Id aus einer Reserve
/// (RAD_ClientUnitRepositoryFindConst). Der Zeiger gilt, solange die Welt
/// besteht.
///
const RAD_ClientUnitRepository_t* RAD_ClientWorldUnits(const RAD_ClientWorld_t *world);

///
/// Meldet "observer" an der Welt an, bzw. den mit "user_argument" ab
/// (model/events/observable.h). false, wenn die Welt schon
/// RAD_MODEL_OBSERVERS_MAX Beobachter hat, das user_argument schon angemeldet
/// ist bzw. beim Abmelden keiner so heisst.
///
bool RAD_ClientWorldSubscribe(const RAD_ClientWorld_t *world, RAD_ClientWorldObserver_t observer);
bool RAD_ClientWorldUnsubscribe(const RAD_ClientWorld_t *world, const void *user_argument);

#endif
