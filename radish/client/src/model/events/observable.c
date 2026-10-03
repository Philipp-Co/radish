#include <radish/model/events/observable.h>

#include <string.h>

bool RAD_ModelObservableSubscribe(RAD_ModelObservable_t *observable, const RAD_ModelObserver_t *observer)
{
    if(observable->number_of_observers >= RAD_MODEL_OBSERVERS_MAX)
    {
        return false;
    }

    for(uint32_t i = 0; i < observable->number_of_observers; ++i)
    {
        if(observable->observers[i].user_argument == observer->user_argument)
        {
            return false;
        }
    }

    observable->observers[observable->number_of_observers] = *observer;
    ++observable->number_of_observers;
    return true;
}

bool RAD_ModelObservableUnsubscribe(RAD_ModelObservable_t *observable, const void *user_argument)
{
    for(uint32_t i = 0; i < observable->number_of_observers; ++i)
    {
        if(observable->observers[i].user_argument != user_argument)
        {
            continue;
        }

        // Die Reihenfolge der uebrigen bleibt, gemeldet wird in Anmeldereihenfolge.
        memmove(&observable->observers[i],
                &observable->observers[i + 1],
                (observable->number_of_observers - i - 1) * sizeof(observable->observers[0]));
        --observable->number_of_observers;
        return true;
    }
    return false;
}

void RAD_ModelObservableClear(RAD_ModelObservable_t *observable)
{
    memset(observable, 0, sizeof(*observable));
}
