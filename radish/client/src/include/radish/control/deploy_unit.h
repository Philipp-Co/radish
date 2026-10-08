#ifndef __RAD_CONTROL_DEPLOY_UNIT_H__
#define __RAD_CONTROL_DEPLOY_UNIT_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/io/net_session.h>
#include <radish/io/net_types.h>

///
/// Eine Einheit aus der Reserve des eigenen Spielers aufs Feld stellen.
///
/// **Der Client schickt nur das Kommando**; ob die Einheit wirklich deployt
/// wird, entscheidet der Server (RAD_GameCheckDeployUnit im Spiel). Steht sie
/// danach auf dem Feld, erfaehrt der Client das wie jede Aenderung an der Welt
/// ueber die Ereignisse des Servers -- und nicht hier.
///

///
/// Schickt das Kommando, die Einheit "unit" auf das Feld (x, y) zu stellen, in
/// Weltkoordinaten. Die Sequenznummer nimmt es aus "session" und zaehlt dort
/// weiter (next_sequence, io/net_session.h). Als Absender steht die eigene
/// Spieler-Id aus "session" darin -- gelesen wird sie vom Server nicht
/// (protobuf/command.proto).
///
/// false, wenn sich das Kommando nicht packen liess; dann ist nichts
/// verschickt (RAD_IoNetRequestDeploy), die Sequenznummer aber verbraucht.
///
bool RAD_ControlDeployUnit(RAD_IoNetSession_t *session, RAD_NetEntityId_t unit, int16_t x, int16_t y);

#endif
