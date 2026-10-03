#include <radish/model/reserve.h>

#include <stddef.h>

typedef void (*RAD_ClientReserveCallback_t)(void *user_argument, const RAD_ClientReserve_t *reserve, const RAD_ClientUnit_t *unit);

static void RAD_ClientReserveNotify(const RAD_ClientReserve_t *reserve, RAD_ClientReserveEvent_t event, const RAD_ClientUnit_t *unit)
{
    // Eine Kopie: ein Beobachter darf sich im Callback an- und abmelden.
    const RAD_ModelObservable_t observable = reserve->observable;

    for(uint32_t i = 0; i < observable.number_of_observers; ++i)
    {
        const RAD_ModelObserver_t *observer = &observable.observers[i];
        const RAD_ClientReserveCallback_t callback = (RAD_ClientReserveCallback_t)observer->callbacks[event];
        if(callback != NULL)
        {
            callback(observer->user_argument, reserve, unit);
        }
    }
}

bool RAD_ClientReserveAddUnit(RAD_ClientReserve_t *reserve, const RAD_ClientUnit_t *unit)
{
    RAD_ClientUnit_t *existing = RAD_ClientReserveFindUnit(reserve, unit->id);
    if(existing != NULL)
    {
        RAD_ClientUnitAssign(existing, unit);
        RAD_ClientReserveNotify(reserve, RAD_CLIENT_RESERVE_EVENT_UNIT_CHANGED, existing);
        return true;
    }

    if(reserve->number_of_units >= RAD_CLIENT_RESERVE_UNITS_MAX)
    {
        return false;
    }

    // Ein Platz wird nie wieder frei, der neue hat also noch keine Beobachter --
    // Assign meldet niemandem etwas und uebernimmt keine fremden.
    RAD_ClientUnit_t *added = &reserve->units[reserve->number_of_units];
    RAD_ClientUnitAssign(added, unit);
    ++reserve->number_of_units;

    RAD_ClientReserveNotify(reserve, RAD_CLIENT_RESERVE_EVENT_UNIT_ADDED, added);
    return true;
}

RAD_ClientUnit_t* RAD_ClientReserveFindUnit(RAD_ClientReserve_t *reserve, RAD_NetEntityId_t id)
{
    for(uint32_t i = 0; i < reserve->number_of_units; ++i)
    {
        if(reserve->units[i].id == id)
        {
            return &reserve->units[i];
        }
    }
    return NULL;
}

bool RAD_ClientReserveSubscribe(const RAD_ClientReserve_t *reserve, RAD_ClientReserveObserver_t observer)
{
    const RAD_ModelObserver_t generic = {
        .user_argument = observer.user_argument,
        .callbacks = {
            [RAD_CLIENT_RESERVE_EVENT_UNIT_ADDED] = (RAD_ModelCallback_t)observer.unit_added,
            [RAD_CLIENT_RESERVE_EVENT_UNIT_CHANGED] = (RAD_ModelCallback_t)observer.unit_changed
        }
    };
    return RAD_ModelObservableSubscribe(RAD_ModelObservableOf(&reserve->observable), &generic);
}

bool RAD_ClientReserveUnsubscribe(const RAD_ClientReserve_t *reserve, const void *user_argument)
{
    return RAD_ModelObservableUnsubscribe(RAD_ModelObservableOf(&reserve->observable), user_argument);
}
