#ifndef __RAD_GAME_CONTROL_COMMANDS_H__
#define __RAD_GAME_CONTROL_COMMANDS_H__

#include <radish/game/control/command/command.h>
#include <radish/game/model/game.h>

///
/// Die Handler der Kommandos, je einer aus seinem Modul unter control/:
/// aufstellen (deploy_unit.c), ziehen (move_unit.c), angreifen
/// (attack_unit.c), abgeben (end_turn.c). Dort steht zu jedem Kommando auch,
/// was es sonst ausmacht -- seine Regel, soweit es eine eigene hat (game.h).
///
/// **Privat und nur fuer RAD_GameExecuteCommand** (game.c), das nach der Art
/// verteilt. Jeder Handler prueft seine Regel noch einmal, bevor er schreibt --
/// ein Kommando kann auch an der Vorpruefung des Servers vorbei ankommen --, und
/// lehnt still ab.
///

void RAD_GameHandleDeployCommand(RAD_Game_t *game, RAD_UserId_t user, const RAD_CommandDeployUnit_t *deploy);
void RAD_GameHandleMoveCommand(RAD_Game_t *game, RAD_CommandMoveUnit_t *move);
void RAD_GameHandleAttackCommand(RAD_Game_t *game, const RAD_CommandAttack_t *attack);
void RAD_GameHandleEndTurnCommand(RAD_Game_t *game, RAD_UserId_t user);

#endif
