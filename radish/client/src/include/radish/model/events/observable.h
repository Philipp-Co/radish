#ifndef __RAD_MODEL_EVENTS_OBSERVABLE_H__
#define __RAD_MODEL_EVENTS_OBSERVABLE_H__

#include <stdbool.h>
#include <stdint.h>

///
/// model/events/ -- jedes Objekt im Model ist beobachtbar. Jedes traegt dafuer
/// seine eigene Liste von Beobachtern (RAD_ModelObservable_t) und meldet, was
/// sich an ihm aendert. Was in einem Behaelter neu dazukommt, meldet der
/// Behaelter: ein Objekt kann seine eigene Entstehung nicht melden, denn vorher
/// kann sich niemand bei ihm anmelden.
///
/// Dies ist der generische Kern. Die Modelle legen je eine typisierte Schicht
/// darueber (RAD_ClientUnitSubscribe, RAD_ClientTileSubscribe, ...) -- ausserhalb
/// des Models nennt niemand diese Datei.
///
/// **Ein Beobachter ist eine Reihe von Callbacks**, je Ereignis einer, abgelegt
/// als RAD_ModelCallback_t und indiziert mit dem Ereignis-Enum des Modells. Die
/// typisierte Schicht wandelt beim Anmelden in RAD_ModelCallback_t und beim
/// Melden zurueck in den Typ, mit dem der Callback angemeldet wurde -- nur so
/// darf er gerufen werden. Ein NULL-Callback heisst: dieses Ereignis nicht.
///
/// **Alles in festen Feldern, nichts auf dem Heap**, wie die Modelle selbst. Mehr
/// als RAD_MODEL_OBSERVERS_MAX Beobachter je Objekt weist Subscribe ab.
///
/// **Die Liste ist kein Zustand des Modells.** Anmelden und Abmelden aendern an
/// dem, was das Objekt abbildet, nichts -- die typisierten Funktionen nehmen das
/// Objekt deshalb const, wie mutable in C++. Ein Objekt, das selbst als const
/// angelegt wurde, laesst sich damit nicht beobachten; im Model gibt es keines.
///
/// **Melden laeuft ueber eine Kopie der Liste.** Ein Beobachter darf sich in
/// seinem Callback an- und abmelden; wirksam wird das erst beim naechsten
/// Ereignis.
///

/// Beobachter je Objekt.
#define RAD_MODEL_OBSERVERS_MAX 4

/// Ereignisse je Modell, das groesste Ereignis-Enum gibt sie vor.
#define RAD_MODEL_EVENTS_MAX 3

///
/// Ein Callback ohne seinen Typ. Nie so zu rufen -- erst zurueck in den Typ, mit
/// dem er angemeldet wurde.
///
typedef void (*RAD_ModelCallback_t)(void);

typedef struct
{
    ///
    /// Wird jedem Callback mitgegeben und kennzeichnet den Beobachter: je Objekt
    /// darf jeder Wert nur einmal angemeldet sein, NULL eingeschlossen.
    ///
    void *user_argument;

    RAD_ModelCallback_t callbacks[RAD_MODEL_EVENTS_MAX];
} RAD_ModelObserver_t;

typedef struct
{
    RAD_ModelObserver_t observers[RAD_MODEL_OBSERVERS_MAX];
    uint32_t number_of_observers;
} RAD_ModelObservable_t;

///
/// Meldet "observer" an. false, wenn schon RAD_MODEL_OBSERVERS_MAX angemeldet sind
/// oder einer mit demselben user_argument -- dann bleibt alles, wie es war.
///
bool RAD_ModelObservableSubscribe(RAD_ModelObservable_t *observable, const RAD_ModelObserver_t *observer);

///
/// Meldet den Beobachter mit "user_argument" ab. false, wenn keiner so heisst.
///
bool RAD_ModelObservableUnsubscribe(RAD_ModelObservable_t *observable, const void *user_argument);

///
/// Meldet alle Beobachter ab -- fuer ein Objekt, das es nicht mehr gibt.
///
void RAD_ModelObservableClear(RAD_ModelObservable_t *observable);

///
/// Die Liste eines const-Objekts zum An- und Abmelden (siehe oben).
///
static inline RAD_ModelObservable_t* RAD_ModelObservableOf(const RAD_ModelObservable_t *observable)
{
    return (RAD_ModelObservable_t*)observable;
}

#endif
