#ifndef __RAD_MODEL_WORLD_H__
#define __RAD_MODEL_WORLD_H__

#include <stdbool.h>
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
/// Spieler einer Partie, und damit Reserven in der Welt.
///
#define RAD_CLIENT_WORLD_PLAYERS 2

typedef struct
{
    RAD_ClientTile_t tiles[RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];

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

    /// Wer die Welt beobachtet -- nur ueber RAD_ClientWorldSubscribe.
    RAD_ModelObservable_t observable;
} RAD_ClientWorld_t;

typedef enum
{
    RAD_CLIENT_WORLD_EVENT_TILE_CREATED = 0,
    RAD_CLIENT_WORLD_EVENT_RESERVE_CREATED,
    RAD_CLIENT_WORLD_EVENT_SIZE_CHANGED
} RAD_ClientWorldEvent_t;

///
/// Ein Beobachter der Welt. "world" ist die Welt selbst; "tile" und "reserve"
/// stehen in ihr und gelten, solange sie nicht neu initialisiert wird. Ein
/// NULL-Callback heisst: dieses Ereignis nicht.
///
/// In "tile_created" und "reserve_created" kann sich der Beobachter gleich am
/// neuen Objekt anmelden -- es hat bis dahin noch nichts gemeldet.
///
typedef struct
{
    void *user_argument;
    void (*tile_created)(void *user_argument, const RAD_ClientWorld_t *world, const RAD_ClientTile_t *tile);
    void (*reserve_created)(void *user_argument, const RAD_ClientWorld_t *world, const RAD_ClientReserve_t *reserve);
    void (*size_changed)(void *user_argument, const RAD_ClientWorld_t *world);
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
/// darauf; sonst bleibt es ohne (siehe oben).
///
void RAD_ClientWorldApplyTile(RAD_ClientWorld_t *world,
                              RAD_ClientPosition_t position,
                              RAD_ClientTileType_t type,
                              RAD_ClientUnitId_t unit);

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
/// Setzt die Einheit "unit" von "from" auf "to" -- Start und Ende eines Weges.
/// Fuer einen Zug, den der Server bestaetigt hat -- er schickt die Aenderung der
/// Felder bislang nicht als Tile-Ereignis mit, also traegt der Client sie selbst
/// nach.
///
/// Beide Felder melden "changed" (RAD_ClientTileSetUnit). Steht die Einheit
/// nicht auf "from", geschieht nichts.
///
void RAD_ClientWorldMoveEntity(RAD_ClientWorld_t *world, RAD_ClientUnitId_t unit, RAD_ClientPosition_t from, RAD_ClientPosition_t to);

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
