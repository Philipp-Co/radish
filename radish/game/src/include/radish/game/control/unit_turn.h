#ifndef __RAD_GAME_CONTROL_UNIT_TURN_H__
#define __RAD_GAME_CONTROL_UNIT_TURN_H__

#include <radish/game/game.h>
#include <radish/game/model/unit/unit.h>

///
/// Die Buchfuehrung darueber, was eine Einheit im laufenden Zug getan hat
/// (RAD_Unit_t.turn), soweit sie Ziehen und Angreifen gemeinsam ist. Gesetzt
/// werden die Flags von den Kommandos selbst (control/deploy_unit.c,
/// move_unit.c, attack_unit.c); hier stehen die gemeinsame Frage und das
/// Zuruecksetzen.
///

///
/// Was Ziehen und Angreifen gemeinsam verlangen: die Einheit gibt es (sonst
/// RAD_GAME_ERROR_NO_UNIT), sie steht auf dem Feld
/// (RAD_GAME_ERROR_UNIT_NOT_DEPLOYED) und ist nicht in diesem Zug aufgestellt
/// worden (RAD_GAME_ERROR_UNIT_JUST_DEPLOYED). "unit" darf NULL sein.
///
RAD_GameResult_t RAD_GameCheckUnitMayAct(const RAD_Unit_t *unit);

///
/// Ein neuer Zug beginnt: alle Einheiten vergessen, was sie im letzten getan
/// haben. Zu rufen bei jedem Zugwechsel, vor dessen Meldung (player.c).
///
void RAD_GameStartTurn(RAD_Game_t *game);

#endif
