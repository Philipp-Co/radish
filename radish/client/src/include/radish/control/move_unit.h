#ifndef __RAD_CONTROL_MOVE_UNIT_H__
#define __RAD_CONTROL_MOVE_UNIT_H__

#include <stdbool.h>
#include <radish/io/net_session.h>
#include <radish/io/net_types.h>

///
/// Eine Einheit des eigenen Spielers einen Weg entlang ziehen lassen.
///
/// **Der Client schickt nur das Kommando**, wie beim Deployen
/// (control/deploy_unit.h): ob und wie weit die Einheit zieht, entscheidet der
/// Server. Bestaetigt er das Kommando, setzt der Client die Einheit aufs Ziel
/// (io/net_event_handler/command_response.c) -- und nicht hier.
///

///
/// Schickt das Kommando, die Einheit "unit" den Weg "path" entlang zu ziehen
/// (RAD_NetPath_t: steps_to[0] ist ihr Standort, mindestens zwei Felder, in
/// Weltkoordinaten). Sequenznummer und Absender wie bei RAD_ControlDeployUnit
/// aus "session".
///
/// false, wenn sich das Kommando nicht packen liess -- etwa ein Weg mit
/// weniger als zwei oder mehr als RAD_NET_PATH_MAX_STEPS Feldern; dann ist
/// nichts verschickt (RAD_IoNetRequestMove), die Sequenznummer aber
/// verbraucht.
///
bool RAD_ControlMoveUnit(RAD_IoNetSession_t *session, RAD_NetEntityId_t unit, const RAD_NetPath_t *path);

#endif
