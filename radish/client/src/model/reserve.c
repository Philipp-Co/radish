#include <radish/model/reserve.h>

#include <stddef.h>

typedef void (*RAD_ClientReserveCallback_t)(void *user_argument, const RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id);

static void RAD_ClientReserveNotify(const RAD_ClientReserve_t *reserve, RAD_ClientReserveEvent_t event, RAD_ClientUnitId_t id)
{
    // Eine Kopie: ein Beobachter darf sich im Callback an- und abmelden.
    const RAD_ModelObservable_t observable = reserve->observable;

    for(uint32_t i = 0; i < observable.number_of_observers; ++i)
    {
        const RAD_ModelObserver_t *observer = &observable.observers[i];
        const RAD_ClientReserveCallback_t callback = (RAD_ClientReserveCallback_t)observer->callbacks[event];
        if(callback != NULL)
        {
            callback(observer->user_argument, reserve, id);
        }
    }
}

///
/// Die Stelle von "id" in der Liste, oder -1.
///
static int32_t RAD_ClientReserveIndexOf(const RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id)
{
    for(uint32_t i = 0; i < reserve->number_of_units; ++i)
    {
        if(reserve->ids[i] == id)
        {
            return (int32_t)i;
        }
    }
    return -1;
}

bool RAD_ClientReserveAddUnit(RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id)
{
    if(id < 0)
    {
        return false;
    }
    if(RAD_ClientReserveIndexOf(reserve, id) >= 0)
    {
        return true;
    }
    if(reserve->number_of_units >= RAD_CLIENT_RESERVE_UNITS_MAX)
    {
        return false;
    }

    reserve->ids[reserve->number_of_units] = id;
    ++reserve->number_of_units;

    RAD_ClientReserveNotify(reserve, RAD_CLIENT_RESERVE_EVENT_UNIT_ADDED, id);
    return true;
}

bool RAD_ClientReserveRemoveUnit(RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id)
{
    const int32_t index = RAD_ClientReserveIndexOf(reserve, id);
    if(index < 0)
    {
        return false;
    }

    for(uint32_t i = (uint32_t)index + 1; i < reserve->number_of_units; ++i)
    {
        reserve->ids[i - 1] = reserve->ids[i];
    }
    --reserve->number_of_units;

    RAD_ClientReserveNotify(reserve, RAD_CLIENT_RESERVE_EVENT_UNIT_REMOVED, id);
    return true;
}

bool RAD_ClientReserveContains(const RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id)
{
    return RAD_ClientReserveIndexOf(reserve, id) >= 0;
}

uint32_t RAD_ClientReserveNumberOfUnits(const RAD_ClientReserve_t *reserve)
{
    return reserve->number_of_units;
}

RAD_ClientUnitId_t RAD_ClientReserveUnitAt(const RAD_ClientReserve_t *reserve, size_t index)
{
    if(index >= reserve->number_of_units)
    {
        return RAD_CLIENT_UNIT_ID_NONE;
    }
    return reserve->ids[index];
}

bool RAD_ClientReserveSubscribe(const RAD_ClientReserve_t *reserve, RAD_ClientReserveObserver_t observer)
{
    const RAD_ModelObserver_t generic = {
        .user_argument = observer.user_argument,
        .callbacks = {
            [RAD_CLIENT_RESERVE_EVENT_UNIT_ADDED] = (RAD_ModelCallback_t)observer.unit_added,
            [RAD_CLIENT_RESERVE_EVENT_UNIT_REMOVED] = (RAD_ModelCallback_t)observer.unit_removed
        }
    };
    return RAD_ModelObservableSubscribe(RAD_ModelObservableOf(&reserve->observable), &generic);
}

bool RAD_ClientReserveUnsubscribe(const RAD_ClientReserve_t *reserve, const void *user_argument)
{
    return RAD_ModelObservableUnsubscribe(RAD_ModelObservableOf(&reserve->observable), user_argument);
}
