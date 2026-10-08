#ifndef __RAD_RESERVE_H__
#define __RAD_RESERVE_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/model/unit/unit.h>

///
/// Eine Reserve: eine Liste von Einheiten, die (noch) auf keinem Feld stehen.
///
/// **Sie fuehrt Buch und besitzt nichts.** Die Reserve haelt Zeiger auf
/// Einheiten, die im Einheitenpool stehen (unit_pool/unit_pool.h) -- dort
/// entstehen sie und dort werden sie entfernt. Hinzufuegen und Entfernen
/// aendern an der Einheit nichts, auch nicht ihren Zustand; und wer eine
/// Einheit aus dem Pool entfernt, nimmt sie vorher aus jeder Reserve, sonst
/// steht darin ein baumelnder Zeiger.
///
/// **Sie kennt keinen Spieler.** Wem eine Reserve gehoert und welche Einheit in
/// welche darf, entscheidet, wer sie haelt. Es kann beliebig viele Reserven
/// nebeneinander geben, und eine Einheit darf in mehreren stehen -- auch das
/// prueft die Reserve nicht.
///
/// **Eine dichte Liste.** Eine Einheit kommt hinten dazu; wird eine entfernt,
/// ruecken die folgenden nach -- wie die Reserve im Client (client/src/model/
/// reserve.c).
///

///
/// Die Reserve selbst: nur ein Name, wie RAD_UnitPool_t. Sie hat Platz fuer
/// RAD_MAX_UNITS Einheiten (game_definitions.h) -- mehr gibt es im Pool nicht.
///
typedef struct RAD_Reserve RAD_Reserve_t;

///
/// Legt eine leere Reserve an; NULL, wenn kein Speicher da ist.
///
/// RAD_DestroyReserve gibt sie wieder her und nullt den Zeiger des Aufrufers.
/// Die Einheiten darin bleiben, wo sie sind -- im Pool. Ein NULL-Zeiger ist
/// kein Fehler.
///
RAD_Reserve_t* RAD_CreateReserve(void);
void RAD_DestroyReserve(RAD_Reserve_t **reserve);

///
/// Haengt "unit" hinten an. Steht sie schon darin, bleibt alles, wie es war, und
/// true kommt zurueck. false, wenn "reserve" oder "unit" NULL ist oder die
/// Reserve voll -- dann aendert sich nichts.
///
bool RAD_ReserveAddUnit(RAD_Reserve_t *reserve, RAD_Unit_t *unit);

///
/// Nimmt "unit" aus der Reserve; die folgenden ruecken nach. false, wenn sie
/// nicht darin steht oder ein Argument NULL ist.
///
bool RAD_ReserveRemoveUnit(RAD_Reserve_t *reserve, const RAD_Unit_t *unit);

///
/// Ob "unit" in der Reserve steht. false fuer NULL.
///
bool RAD_ReserveContains(const RAD_Reserve_t *reserve, const RAD_Unit_t *unit);

///
/// Wie viele Einheiten in der Reserve stehen. 0 fuer eine NULL-Reserve.
///
int32_t RAD_ReserveNumberOfUnits(const RAD_Reserve_t *reserve);

///
/// Die Einheit an Stelle "index" in [0, RAD_ReserveNumberOfUnits), oder NULL
/// ausserhalb. Ein Index gilt nur, bis eine Einheit dazukommt oder wegfaellt.
///
RAD_Unit_t* RAD_ReserveUnitAt(const RAD_Reserve_t *reserve, int32_t index);

#endif
