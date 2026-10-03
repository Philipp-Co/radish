#ifndef __RAD_MODEL_TILE_H__
#define __RAD_MODEL_TILE_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/io/net_types.h>
#include <radish/model/events/observable.h>
#include <radish/model/unit.h>

///
/// Was der Client von einem Feld weiss -- und nur das.
///
/// **Ein Abbild, keine Simulation**, wie Welt und Einheit (model/world.h,
/// model/unit.h): gefuellt wird es aus dem, was der Server schickt
/// (RAD_NetTile_t); Regeln wendet es keine an.
///
/// **Die Einheit steht als Zeiger darin.** Sie gehoert nicht dem Feld, sondern
/// dem, der die Einheiten haelt; das Feld zeigt nur auf sie. Der Zeiger gilt
/// also nur, solange die Einheit an ihrem Platz bleibt -- wer sie haelt, darf
/// sie nicht verschieben oder freigeben, solange ein Feld auf sie zeigt.
///
/// **Beobachtbar** (model/events/observable.h): ein Feld meldet "changed", wenn
/// es neu uebernommen wird oder eine andere Einheit bekommt, und "removed", wenn
/// die Welt es verwirft. Dass es ein Feld neu gibt, meldet die Welt.
///
/// **Nicht per Zuweisung ersetzen**, aus demselben Grund wie bei der Einheit:
/// "*a = *b" ueberschriebe die Beobachter.
///
typedef struct
{
    ///
    /// Position in der Welt. Redundant zur Position im Raster, aber noetig, sobald
    /// ein einzelnes Feld weitergereicht wird -- wie RAD_Tile_t im Spiel.
    ///
    int32_t x;
    int32_t y;

    RAD_NetTileType_t type;

    /// Die Einheit auf diesem Feld, NULL, wenn es frei ist.
    RAD_ClientUnit_t *unit;

    /// Wer das Feld beobachtet -- nur ueber RAD_ClientTileSubscribe.
    RAD_ModelObservable_t observable;
} RAD_ClientTile_t;

typedef enum
{
    RAD_CLIENT_TILE_EVENT_CHANGED = 0,
    RAD_CLIENT_TILE_EVENT_REMOVED
} RAD_ClientTileEvent_t;

///
/// Ein Beobachter eines Feldes. "tile" ist das beobachtete Feld selbst. Ein
/// NULL-Callback heisst: dieses Ereignis nicht.
///
/// Nach "removed" ist der Beobachter abgemeldet -- das Feld gibt es nicht mehr.
///
typedef struct
{
    void *user_argument;
    void (*changed)(void *user_argument, const RAD_ClientTile_t *tile);
    void (*removed)(void *user_argument, const RAD_ClientTile_t *tile);
} RAD_ClientTileObserver_t;

///
/// Uebernimmt das Feld "net", wie es der Server schickt, nach "tile" -- es
/// ersetzt, was dort vorher stand, ausser den Beobachtern -- setzt die Einheit
/// auf "unit" und meldet "changed".
///
/// **Die Einheit sucht das Feld nicht selbst.** Der Server nennt sie nur mit
/// ihrer Id (net->entity_id); die passende Einheit schlaegt der Aufrufer nach,
/// der die Einheiten haelt. NULL fuer ein freies Feld.
///
void RAD_ClientTileFromNet(RAD_ClientTile_t *tile, const RAD_NetTile_t *net, RAD_ClientUnit_t *unit);

///
/// Setzt die Einheit auf "unit", NULL fuer frei, und meldet "changed" -- nur,
/// wenn es eine andere ist.
///
void RAD_ClientTileSetUnit(RAD_ClientTile_t *tile, RAD_ClientUnit_t *unit);

///
/// Meldet "removed" und danach alle Beobachter ab. Fuer die Welt, wenn sie das
/// Feld verwirft; an den Daten aendert es nichts.
///
void RAD_ClientTileRemove(RAD_ClientTile_t *tile);

///
/// Ob auf dem Feld eine Einheit steht.
///
bool RAD_ClientTileHasUnit(const RAD_ClientTile_t *tile);

///
/// Meldet "observer" an dem Feld an, bzw. den mit "user_argument" ab
/// (model/events/observable.h). false, wenn das Feld schon
/// RAD_MODEL_OBSERVERS_MAX Beobachter hat, das user_argument schon angemeldet
/// ist bzw. beim Abmelden keiner so heisst.
///
bool RAD_ClientTileSubscribe(const RAD_ClientTile_t *tile, RAD_ClientTileObserver_t observer);
bool RAD_ClientTileUnsubscribe(const RAD_ClientTile_t *tile, const void *user_argument);

#endif
