#ifndef __RAD_PLAYER_H__
#define __RAD_PLAYER_H__

#include <stdint.h>
#include <stdbool.h>
#include <radish/game/user.h>
#include <radish/game/model/unit/unit.h>
#include <radish/game/model/reserve/reserve.h>

///
/// Ein Spieler: seine Id, seine Einheitenliste und seine Reserve.
///
/// **Die Id ist die des Benutzers** (RAD_UserId_t, user.h) -- wer vor dem Client
/// sitzt. Sie wird beim Anlegen gesetzt und bleibt; RAD_USER_NONE ist kein
/// Spieler.
///
/// **Die Einheitenliste sind alle Einheiten des Spielers**, gleich wo sie stehen:
/// in der Reserve, auf dem Feld, zerstoert. Die Reserve ist davon der Teil, der
/// auf keinem Feld steht. Beide halten nur Zeiger in den Einheitenpool
/// (unit_pool/unit_pool.h) und besitzen nichts -- wie die Reserve selbst
/// (reserve/reserve.h).
///
/// **Was der Spieler zusichert:** eine Einheit, die aus seiner Liste geht, geht
/// auch aus seiner Reserve (RAD_PlayerRemoveUnit). Umgekehrt prueft er nicht,
/// ob, was in die Reserve kommt, auch in der Liste steht -- die Reserve wird
/// direkt ueber ihr Modul gefuehrt (RAD_PlayerReserve).
///
/// RAD_Unit_t.owner fasst der Spieler nicht an. Wem eine Einheit gehoert und in
/// welche Liste sie damit kommt, haelt zusammen, wer beides fuehrt.
///

///
/// Der Spieler selbst: nur ein Name, wie RAD_UnitPool_t und RAD_Reserve_t.
///
typedef struct RAD_Player RAD_Player_t;

///
/// Legt einen Spieler mit der Id "id", leerer Einheitenliste und leerer Reserve
/// an. NULL fuer RAD_USER_NONE oder wenn kein Speicher da ist.
///
/// RAD_DestroyPlayer gibt ihn samt seiner Reserve wieder her und nullt den
/// Zeiger des Aufrufers. Die Einheiten bleiben im Pool. Ein NULL-Zeiger ist kein
/// Fehler.
///
RAD_Player_t* RAD_CreatePlayer(RAD_UserId_t id);
void RAD_DestroyPlayer(RAD_Player_t **player);

///
/// Die Id des Spielers; RAD_USER_NONE fuer NULL.
///
RAD_UserId_t RAD_PlayerId(const RAD_Player_t *player);

///
/// Die Reserve des Spielers, gefuehrt ueber reserve/reserve.h. Sie gehoert dem
/// Spieler und lebt so lange wie er -- nicht selbst zerstoeren. NULL fuer NULL.
///
RAD_Reserve_t* RAD_PlayerReserve(const RAD_Player_t *player);

///
/// Die Einheitenliste, nach demselben Muster wie die Reserve: eine dichte Liste,
/// hinten angehaengt, beim Entfernen rueckt nach.
///
/// RAD_PlayerAddUnit: steht "unit" schon darin, bleibt alles, wie es war, und
/// true kommt zurueck. false fuer NULL oder eine volle Liste (RAD_MAX_UNITS).
///
/// RAD_PlayerRemoveUnit nimmt "unit" aus der Liste **und aus der Reserve**.
/// false, wenn sie nicht in der Liste steht oder ein Argument NULL ist.
///
/// RAD_PlayerUnitAt liefert NULL ausserhalb [0, RAD_PlayerNumberOfUnits). Ein
/// Index gilt nur, bis eine Einheit dazukommt oder wegfaellt.
///
bool RAD_PlayerAddUnit(RAD_Player_t *player, RAD_Unit_t *unit);
bool RAD_PlayerRemoveUnit(RAD_Player_t *player, const RAD_Unit_t *unit);
bool RAD_PlayerOwnsUnit(const RAD_Player_t *player, const RAD_Unit_t *unit);
int32_t RAD_PlayerNumberOfUnits(const RAD_Player_t *player);
RAD_Unit_t* RAD_PlayerUnitAt(const RAD_Player_t *player, int32_t index);

#endif
