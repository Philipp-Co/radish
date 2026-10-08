#ifndef __RAD_UNIT_POOL_H__
#define __RAD_UNIT_POOL_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/model/model.h>
#include <radish/game/model/unit/unit.h>

///
/// Der Einheitenpool: der Speicher, in dem die Einheiten eines Spiels stehen, und
/// **der einzige Ort, an dem eine Einheit entsteht.** Wer eine Figur braucht,
/// holt sie sich hier (RAD_UnitPoolAddUnit) und bekommt einen Zeiger in den Pool;
/// eine RAD_Unit_t auf dem Stapel oder im Heap ist keine Einheit des Spiels, nur
/// ein Satz Werte. Durchsetzen kann C das nicht -- die Struktur ist oeffentlich
/// (unit.h), weil die Ereignisse sie nach draussen reichen. Was der Pool
/// durchsetzt, ist die Id: vergeben wird sie nur hier.
///
/// **Gearbeitet wird mit Zeigern.** Anlegen liefert einen Zeiger auf den Platz im
/// Pool, entfernen nimmt ihn wieder. Der Zeiger bleibt gueltig, bis die Einheit
/// entfernt oder der Pool zerstoert wird -- der Pool verschiebt nichts, eine
/// Einheit bleibt auf ihrem Platz.
///
/// **Ein Platz wird wieder frei, eine Id nicht.** Die Id zaehlt fuer sich hoch und
/// ist nicht der Index des Platzes: so trifft ein Kommando, das eine entfernte
/// Einheit nennt, nie die neue auf ihrem alten Platz (dieselbe Zusage wie
/// RAD_Unit_t.id, unit.h).
///
/// Das Spiel besitzt einen Pool (model/game.h); die Welt leiht ihn sich und fuehrt
/// darin Position und Zustand (world/world.h), die Spieler zeigen mit
/// Einheitenliste und Reserve hinein (player/player.h).
///

///
/// Der Pool selbst: nur ein Name, wie RAD_EventManager_t. Was er haelt -- Platz
/// fuer RAD_MAX_UNITS Einheiten (game_definitions.h) --, steht in unit_pool.c.
///
typedef struct RAD_UnitPool RAD_UnitPool_t;

///
/// Legt einen leeren Pool an; NULL, wenn kein Speicher da ist.
///
/// RAD_DestroyUnitPool gibt ihn wieder her und nullt den Zeiger des Aufrufers,
/// wie RAD_DestroyEventManager. Jeder Zeiger auf eine Einheit darin baumelt
/// danach. Ein NULL-Pool -- oder ein Zeiger darauf -- ist kein Fehler.
///
RAD_UnitPool_t* RAD_CreateUnitPool(void);
void RAD_DestroyUnitPool(RAD_UnitPool_t **pool);

///
/// Legt eine Einheit an und liefert den Zeiger auf ihren Platz. Gesetzt sind die
/// Id (die naechste, die der Pool noch nicht vergeben hat), der Zustand
/// RAD_UNIT_STATE_RESERVE, kein Besitzer (RAD_USER_NONE) und keine Position
/// (-1,-1); alles andere ist 0 und wird ueber den Zeiger gefuellt.
///
/// NULL, wenn "pool" NULL ist, kein Platz mehr frei ist oder die Ids
/// ausgegangen sind. Der Pool aendert sich dann nicht.
///
RAD_Unit_t* RAD_UnitPoolAddUnit(RAD_UnitPool_t *pool);

///
/// Entfernt die Einheit, auf die "*unit" zeigt, gibt ihren Platz frei und nullt
/// den Zeiger des Aufrufers. Jeder andere Zeiger auf sie baumelt danach.
///
/// false und keine Aenderung, wenn "pool", "unit" oder "*unit" NULL ist oder
/// "*unit" auf keine Einheit in diesem Pool zeigt -- auch auf einen Platz, der
/// schon frei ist. Ein Zeiger von aussen wird damit abgewiesen und nicht
/// geschrieben.
///
bool RAD_UnitPoolRemoveUnit(RAD_UnitPool_t *pool, RAD_Unit_t **unit);

///
/// Wie viele Einheiten im Pool stehen. 0 fuer einen NULL-Pool.
///
int32_t RAD_UnitPoolNumberOfUnits(const RAD_UnitPool_t *pool);

///
/// Die Einheit mit der Id "id", oder NULL, wenn keine sie traegt. Gesucht wird
/// ueber die Plaetze -- die Id ist nicht der Index (oben).
///
RAD_Unit_t* RAD_UnitPoolUnitById(const RAD_UnitPool_t *pool, RAD_UnitId_t id);

///
/// Die Einheit an Stelle "index" in [0, RAD_UnitPoolNumberOfUnits), oder NULL
/// ausserhalb. Gezaehlt wird dicht ueber die belegten Plaetze in ihrer
/// Reihenfolge. Solange nichts aus dem Pool entfernt wurde, ist das die
/// Reihenfolge der Ids -- ein frei gewordener Platz wird vor den hinteren
/// wieder belegt. Ein Index gilt nur, bis eine Einheit dazukommt oder wegfaellt.
///
RAD_Unit_t* RAD_UnitPoolUnitAt(const RAD_UnitPool_t *pool, int32_t index);

#endif
