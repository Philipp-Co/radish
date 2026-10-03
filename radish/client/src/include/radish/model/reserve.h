#ifndef __RAD_MODEL_RESERVE_H__
#define __RAD_MODEL_RESERVE_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/io/net_types.h>
#include <radish/model/events/observable.h>
#include <radish/model/unit.h>

///
/// Einheiten in der Reserve eines Spielers, wie RAD_MAX_UNITS im Spiel -- dort
/// fuer alle Spieler zusammen, mehr kann einer allein also nicht haben.
///
#define RAD_CLIENT_RESERVE_UNITS_MAX 64

///
/// Die Einheiten eines Spielers, die dem Spiel bekannt sind, aber auf keinem Feld
/// stehen. Die Welt haelt eine je Spieler (model/world.h) und vergibt sie an die
/// Besitzer.
///
/// **Die Einheiten bleiben an ihrem Platz.** Eine Einheit wird ersetzt, aber nie
/// verschoben -- ein Zeiger auf sie (RAD_ClientTile_t.unit, ein Beobachter der
/// Einheit) bleibt gueltig, solange die Welt nicht neu initialisiert wird.
///
/// **Beobachtbar** (model/events/observable.h): eine Reserve meldet
/// "unit_added", wenn eine Einheit neu in sie kommt, und "unit_changed", wenn
/// eine, die schon darin steht, ersetzt wird. Die Einheit selbst meldet
/// daneben ihr eigenes "changed".
///
typedef struct
{
    /// Wem die Reserve gehoert, RAD_NET_USER_NONE fuer eine freie.
    RAD_NetUserId_t owner;

    RAD_ClientUnit_t units[RAD_CLIENT_RESERVE_UNITS_MAX];
    uint32_t number_of_units;

    /// Wer die Reserve beobachtet -- nur ueber RAD_ClientReserveSubscribe.
    RAD_ModelObservable_t observable;
} RAD_ClientReserve_t;

typedef enum
{
    RAD_CLIENT_RESERVE_EVENT_UNIT_ADDED = 0,
    RAD_CLIENT_RESERVE_EVENT_UNIT_CHANGED
} RAD_ClientReserveEvent_t;

///
/// Ein Beobachter einer Reserve. "reserve" ist die beobachtete Reserve selbst,
/// "unit" die Einheit darin, um die es geht. Ein NULL-Callback heisst: dieses
/// Ereignis nicht.
///
typedef struct
{
    void *user_argument;
    void (*unit_added)(void *user_argument, const RAD_ClientReserve_t *reserve, const RAD_ClientUnit_t *unit);
    void (*unit_changed)(void *user_argument, const RAD_ClientReserve_t *reserve, const RAD_ClientUnit_t *unit);
} RAD_ClientReserveObserver_t;

///
/// Legt "unit" in die Reserve -- **ohne auf den Besitzer zu sehen**, den
/// vergleicht die Welt. Steht eine Einheit mit derselben Id schon darin, wird sie
/// an ihrem Platz ersetzt (RAD_ClientUnitAssign) und "unit_changed" gemeldet,
/// sonst kommt sie hinten dazu und "unit_added" wird gemeldet.
///
/// false, wenn die Reserve voll ist -- dann bleibt alles, wie es war.
///
bool RAD_ClientReserveAddUnit(RAD_ClientReserve_t *reserve, const RAD_ClientUnit_t *unit);

///
/// Die Einheit mit der Id "id", oder NULL, wenn keine in der Reserve so heisst.
///
RAD_ClientUnit_t* RAD_ClientReserveFindUnit(RAD_ClientReserve_t *reserve, RAD_NetEntityId_t id);

///
/// Meldet "observer" an der Reserve an, bzw. den mit "user_argument" ab
/// (model/events/observable.h). false, wenn die Reserve schon
/// RAD_MODEL_OBSERVERS_MAX Beobachter hat, das user_argument schon angemeldet
/// ist bzw. beim Abmelden keiner so heisst.
///
bool RAD_ClientReserveSubscribe(const RAD_ClientReserve_t *reserve, RAD_ClientReserveObserver_t observer);
bool RAD_ClientReserveUnsubscribe(const RAD_ClientReserve_t *reserve, const void *user_argument);

#endif
