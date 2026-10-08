#include <radish/model/unit.h>

#include <string.h>

typedef void (*RAD_ClientUnitCallback_t)(void *user_argument, const RAD_ClientUnit_t *unit);

static void RAD_ClientUnitNotify(const RAD_ClientUnit_t *unit, RAD_ClientUnitEvent_t event)
{
    // Eine Kopie: ein Beobachter darf sich im Callback an- und abmelden.
    const RAD_ModelObservable_t observable = unit->observable;

    for(uint32_t i = 0; i < observable.number_of_observers; ++i)
    {
        const RAD_ModelObserver_t *observer = &observable.observers[i];
        const RAD_ClientUnitCallback_t callback = (RAD_ClientUnitCallback_t)observer->callbacks[event];
        if(callback != NULL)
        {
            callback(observer->user_argument, unit);
        }
    }
}

static void RAD_ClientUnitNotifyChanged(const RAD_ClientUnit_t *unit)
{
    RAD_ClientUnitNotify(unit, RAD_CLIENT_UNIT_EVENT_CHANGED);
}

void RAD_ClientUnitInit(RAD_ClientUnit_t *unit, RAD_ClientUnitId_t id, RAD_ClientPlayerId_t owner, const char *name)
{
    memset(unit, 0, sizeof(*unit));

    unit->id = id;
    unit->owner = owner;

    if(name != NULL)
    {
        strncpy(unit->name, name, sizeof(unit->name) - 1);
    }
}

void RAD_ClientUnitAssign(RAD_ClientUnit_t *unit, const RAD_ClientUnit_t *source)
{
    if(unit == source)
    {
        return;
    }

    const RAD_ModelObservable_t observable = unit->observable;
    *unit = *source;
    unit->observable = observable;

    RAD_ClientUnitNotifyChanged(unit);
}

bool RAD_ClientUnitAddMember(RAD_ClientUnit_t *unit, const RAD_ClientMember_t *member)
{
    if((unit->number_of_members >= RAD_CLIENT_UNIT_MEMBERS_MAX)
       || (member->number_of_weapons > RAD_CLIENT_MEMBER_WEAPONS_MAX))
    {
        return false;
    }

    unit->members[unit->number_of_members] = *member;
    ++unit->number_of_members;

    RAD_ClientUnitNotifyChanged(unit);
    return true;
}

void RAD_ClientUnitClearMembers(RAD_ClientUnit_t *unit)
{
    memset(unit->members, 0, sizeof(unit->members));
    unit->number_of_members = 0;

    RAD_ClientUnitNotifyChanged(unit);
}

void RAD_ClientUnitRemove(RAD_ClientUnit_t *unit)
{
    RAD_ClientUnitNotify(unit, RAD_CLIENT_UNIT_EVENT_REMOVED);
    RAD_ModelObservableClear(&unit->observable);
}

bool RAD_ClientUnitMemberAt(const RAD_ClientUnit_t *unit, size_t index, RAD_ClientMember_t *output)
{
    if(index >= unit->number_of_members)
    {
        return false;
    }

    *output = unit->members[index];
    return true;
}

void RAD_ClientUnitHitPoints(const RAD_ClientUnit_t *unit, int32_t *current, int32_t *maximum)
{
    int32_t sum_current = 0;
    int32_t sum_maximum = 0;

    for(uint32_t i = 0; i < unit->number_of_members; ++i)
    {
        sum_current += unit->members[i].health;
        sum_maximum += unit->members[i].health_max;
    }

    *current = sum_current;
    *maximum = sum_maximum;
}

bool RAD_ClientUnitSubscribe(const RAD_ClientUnit_t *unit, RAD_ClientUnitObserver_t observer)
{
    const RAD_ModelObserver_t generic = {
        .user_argument = observer.user_argument,
        .callbacks = {
            [RAD_CLIENT_UNIT_EVENT_CHANGED] = (RAD_ModelCallback_t)observer.changed,
            [RAD_CLIENT_UNIT_EVENT_REMOVED] = (RAD_ModelCallback_t)observer.removed
        }
    };
    return RAD_ModelObservableSubscribe(RAD_ModelObservableOf(&unit->observable), &generic);
}

bool RAD_ClientUnitUnsubscribe(const RAD_ClientUnit_t *unit, const void *user_argument)
{
    return RAD_ModelObservableUnsubscribe(RAD_ModelObservableOf(&unit->observable), user_argument);
}
