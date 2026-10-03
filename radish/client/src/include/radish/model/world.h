#ifndef __RAD_MODEL_WORLD_H__
#define __RAD_MODEL_WORLD_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/io/net_types.h>
#include <radish/model/events/observable.h>
#include <radish/model/reserve.h>
#include <radish/model/tile.h>
#include <radish/model/unit.h>
#include <radish/rendering/iso_definitions.h>

///
/// Was der Client von der Welt weiss -- und nur das.
///
/// **Ein Abbild, keine Simulation.** Gefuellt wird es aus dem, was der Server
/// schickt: den Feldern einer Discover-Antwort, den Tile-Ereignissen, den
/// Einheiten der Reserve und den Zuegen, die er bestaetigt hat. Regeln wendet es
/// keine an; ob ein Zug geht, entscheidet der Server. Der User-Input liest hier
/// nach, was auf einem Feld liegt, um daraus eine Anfrage zu bauen.
///
/// Das Raster ist so gross wie das der Darstellung (RAD_ISO_MAP_SIZE). Ein Feld
/// ausserhalb davon wird verworfen, genau wie in der Iso-Map.
///
/// **Einheiten auf dem Feld kennt die Welt noch nicht.** Sie haelt nur die
/// Reserven; RAD_ClientTile_t.unit bleibt deshalb vorerst immer NULL, auch wenn
/// der Server auf dem Feld eine Einheit nennt. Solange kann der User-Input keine
/// Einheit auswaehlen.
///
/// **Beobachtbar** (model/events/observable.h), wie alles im Model: die Welt
/// meldet, was in ihr neu dazukommt -- "tile_created", wenn der Server ein Feld
/// zum ersten Mal schickt, "reserve_created", wenn ein Besitzer seine Reserve
/// bekommt -- und "size_changed". Was sich an einem Feld oder einer Reserve
/// aendert, melden die selbst; wer das wissen will, meldet sich dort an.
///
/// **Die Welt gibt Zeiger heraus, keine Kopien** (RAD_ClientWorldTileAt,
/// RAD_ClientWorldReserve): an einer Kopie liesse sich niemand anmelden, der
/// etwas erfahren soll.
///

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
/// Reserven frei und leer, **ohne Beobachter** -- weder an der Welt noch an
/// irgendetwas in ihr. Zeiger in die Welt zeigen danach auf leere Objekte.
///
void RAD_ClientWorldInit(RAD_ClientWorld_t *world);

///
/// Setzt die Groesse und meldet "size_changed" -- nur, wenn sie sich aendert.
///
void RAD_ClientWorldSetSize(RAD_ClientWorld_t *world, uint32_t width, uint32_t height);

///
/// Uebernimmt ein Feld, wie es der Server schickt. Ein Feld, das die Welt noch
/// nicht kannte, meldet sie als "tile_created"; eines, das sie schon kannte,
/// meldet selbst "changed" (RAD_ClientTileFromNet). Ohne Einheit: siehe oben.
///
void RAD_ClientWorldApplyTile(RAD_ClientWorld_t *world, const RAD_NetTile_t *tile);

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
/// Setzt die Figur "entity" vom Start des Weges auf sein Ende. Fuer einen Zug,
/// den der Server bestaetigt hat -- er schickt die Aenderung der Felder bislang
/// nicht als Tile-Ereignis mit, also traegt der Client sie selbst nach.
///
/// Beide Felder melden "changed" (RAD_ClientTileSetUnit). Steht die Figur nicht
/// auf steps_to[0], geschieht nichts.
///
void RAD_ClientWorldMoveEntity(RAD_ClientWorld_t *world, RAD_NetEntityId_t entity, const RAD_NetPath_t *path);

///
/// Legt eine Kopie von "unit" in die Reserve ihres Besitzers (unit->owner). Hat
/// noch keine Reserve diesen Besitzer, bekommt er die erste freie, und die Welt
/// meldet "reserve_created" -- vor dem "unit_added" der Reserve.
///
/// **Eine Einheit, die schon in der Reserve steht** (gleiche Id), wird an ihrem
/// Platz ersetzt -- der Server schickt die Reserve auf jede Anfrage ganz
/// (RAD_ClientReserveAddUnit).
///
/// false, wenn der Besitzer RAD_NET_USER_NONE ist, schon beide Reserven anderen
/// gehoeren oder seine Reserve voll ist -- dann bleibt alles, wie es war.
///
bool RAD_ClientWorldAddReserveUnit(RAD_ClientWorld_t *world, const RAD_ClientUnit_t *unit);

///
/// Die Reserve von "owner", oder NULL, wenn keine ihm gehoert. Der Zeiger gilt,
/// solange die Welt nicht neu initialisiert wird.
///
const RAD_ClientReserve_t* RAD_ClientWorldReserve(const RAD_ClientWorld_t *world, RAD_NetUserId_t owner);

///
/// Meldet "observer" an der Welt an, bzw. den mit "user_argument" ab
/// (model/events/observable.h). false, wenn die Welt schon
/// RAD_MODEL_OBSERVERS_MAX Beobachter hat, das user_argument schon angemeldet
/// ist bzw. beim Abmelden keiner so heisst.
///
bool RAD_ClientWorldSubscribe(const RAD_ClientWorld_t *world, RAD_ClientWorldObserver_t observer);
bool RAD_ClientWorldUnsubscribe(const RAD_ClientWorld_t *world, const void *user_argument);

#endif
