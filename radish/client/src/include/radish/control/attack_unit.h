#ifndef __RAD_CONTROL_ATTACK_UNIT_H__
#define __RAD_CONTROL_ATTACK_UNIT_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/io/net_session.h>
#include <radish/io/net_types.h>

///
/// Eine Einheit des eigenen Spielers ein Feld angreifen lassen.
///
/// **Der Client schickt nur das Kommando**, wie beim Deployen
/// (control/deploy_unit.h): ob die Einheit angreifen darf -- nicht frisch
/// aufgestellt, nicht schon angegriffen --, entscheidet der Server, und seine
/// Antwort kommt als Ereignis (io/net_event_handler/attack_response.c).
///

///
/// Schickt das Kommando, die Einheit "unit" das Feld (x, y) angreifen zu
/// lassen, in Weltkoordinaten. Sequenznummer und Absender wie bei
/// RAD_ControlDeployUnit aus "session".
///
/// false, wenn sich das Kommando nicht packen liess; dann ist nichts
/// verschickt (RAD_IoNetRequestAttack), die Sequenznummer aber verbraucht.
///
bool RAD_ControlAttackUnit(RAD_IoNetSession_t *session, RAD_NetEntityId_t unit, int16_t x, int16_t y);

#endif
