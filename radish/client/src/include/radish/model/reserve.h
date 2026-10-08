#ifndef __RAD_MODEL_RESERVE_H__
#define __RAD_MODEL_RESERVE_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <radish/model/events/observable.h>
#include <radish/model/unit.h>

///
/// Einheiten in der Reserve eines Spielers, wie RAD_MAX_UNITS im Spiel -- dort
/// fuer alle Spieler zusammen, mehr kann einer allein also nicht haben.
///
#define RAD_CLIENT_RESERVE_UNITS_MAX 64

///
/// Welche Einheiten eines Spielers dem Spiel bekannt sind, aber auf keinem Feld
/// stehen. Die Welt haelt eine je Spieler (model/world.h) und vergibt sie an die
/// Besitzer.
///
/// **Nur Ids, keine Einheiten.** Die Reserve haelt eine Liste von Ids; die
/// Einheiten selbst stehen im Unit-Repository der Welt
/// (model/unit_repository.h), dort schlaegt nach, wer mehr als die Id braucht.
///
/// **Eine dichte Liste.** Eine Id kommt hinten dazu; wird eine entfernt, ruecken
/// die folgenden nach. Kommt sie spaeter zurueck, steht sie wieder hinten.
///
/// **Beobachtbar** (model/events/observable.h): eine Reserve meldet
/// "unit_added", wenn eine Id in sie kommt, und "unit_removed", wenn eine sie
/// verlaesst. Was sich an einer Einheit aendert, meldet die Einheit selbst.
///
typedef struct
{
    /// Wem die Reserve gehoert, RAD_CLIENT_PLAYER_NONE fuer eine freie.
    RAD_ClientPlayerId_t owner;

    /// Die Ids der Einheiten darin, "number_of_units" vorne belegt.
    RAD_ClientUnitId_t ids[RAD_CLIENT_RESERVE_UNITS_MAX];
    uint32_t number_of_units;

    /// Wer die Reserve beobachtet -- nur ueber RAD_ClientReserveSubscribe.
    RAD_ModelObservable_t observable;
} RAD_ClientReserve_t;

typedef enum
{
    RAD_CLIENT_RESERVE_EVENT_UNIT_ADDED = 0,
    RAD_CLIENT_RESERVE_EVENT_UNIT_REMOVED
} RAD_ClientReserveEvent_t;

///
/// Ein Beobachter einer Reserve. "reserve" ist die beobachtete Reserve selbst,
/// "id" die Einheit, um die es geht. Ein NULL-Callback heisst: dieses Ereignis
/// nicht.
///
/// Bei "unit_removed" steht "id" schon nicht mehr in der Reserve.
///
typedef struct
{
    void *user_argument;
    void (*unit_added)(void *user_argument, const RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id);
    void (*unit_removed)(void *user_argument, const RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id);
} RAD_ClientReserveObserver_t;

///
/// Haengt "id" an die Reserve an und meldet "unit_added" -- **ohne auf den
/// Besitzer zu sehen**, den vergleicht die Welt.
///
/// Steht "id" schon darin, bleibt alles, wie es war, und true kommt zurueck.
/// false, wenn "id" negativ ist oder die Reserve voll -- dann bleibt alles, wie
/// es war.
///
bool RAD_ClientReserveAddUnit(RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id);

///
/// Nimmt "id" aus der Reserve und meldet "unit_removed"; die folgenden Ids
/// ruecken nach. An der Einheit selbst aendert es nichts.
///
/// false, wenn "id" nicht in der Reserve steht.
///
bool RAD_ClientReserveRemoveUnit(RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id);

///
/// Ob "id" in der Reserve steht.
///
bool RAD_ClientReserveContains(const RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id);

///
/// Wie viele Einheiten in der Reserve stehen.
///
uint32_t RAD_ClientReserveNumberOfUnits(const RAD_ClientReserve_t *reserve);

///
/// Die Id an Stelle "index" in [0, RAD_ClientReserveNumberOfUnits), oder
/// RAD_CLIENT_UNIT_ID_NONE ausserhalb. Ein Index gilt nur, bis eine Einheit
/// dazukommt oder wegfaellt.
///
RAD_ClientUnitId_t RAD_ClientReserveUnitAt(const RAD_ClientReserve_t *reserve, size_t index);

///
/// Meldet "observer" an der Reserve an, bzw. den mit "user_argument" ab
/// (model/events/observable.h). false, wenn die Reserve schon
/// RAD_MODEL_OBSERVERS_MAX Beobachter hat, das user_argument schon angemeldet
/// ist bzw. beim Abmelden keiner so heisst.
///
bool RAD_ClientReserveSubscribe(const RAD_ClientReserve_t *reserve, RAD_ClientReserveObserver_t observer);
bool RAD_ClientReserveUnsubscribe(const RAD_ClientReserve_t *reserve, const void *user_argument);

#endif
