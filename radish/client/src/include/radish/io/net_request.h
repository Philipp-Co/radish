#ifndef __RAD_IO_NET_REQUEST_H__
#define __RAD_IO_NET_REQUEST_H__

#include <stdbool.h>
#include <stdint.h>
#include <radish/io/net_session.h>
#include <radish/io/net_types.h>

///
/// io/net_request/ -- die Anfragen an das Backend, je Anfrage eine Datei. Jede
/// packt ihre Nachricht (io/net_codec.h), verschickt sie (io/net_transport.h),
/// schreibt ins Log und haelt in der Session fest, was die Event-Handler fuer die
/// Antwort brauchen.
///
/// Verschickt wird nur die per Protobuf gepackte NetUserRequest -- was davor
/// steht, Zucchini-Code und Absender, setzt das Backend (siehe own_player_id in
/// io/net_session.h).
///

///
/// Ein Zug (move.c). false, wenn er sich nicht packen liess; dann ist nichts
/// verschickt.
///
bool RAD_IoNetRequestMove(RAD_IoNetSession_t *session, const RAD_NetMoveRequest_t *request);

///
/// Ein Deployment (deploy.c), wie der Zug: false, wenn es sich nicht packen
/// liess; dann ist nichts verschickt.
///
bool RAD_IoNetRequestDeploy(RAD_IoNetSession_t *session, const RAD_NetDeployRequest_t *request);

///
/// Ein Angriff auf ein Feld (attack.c), wie das Deployment: false, wenn er sich
/// nicht packen liess; dann ist nichts verschickt.
///
bool RAD_IoNetRequestAttack(RAD_IoNetSession_t *session, const RAD_NetAttackRequest_t *request);

///
/// Das Abgeben des Zuges (end_turn.c), wie das Deployment: false, wenn es sich
/// nicht packen liess; dann ist nichts verschickt.
///
bool RAD_IoNetRequestEndTurn(RAD_IoNetSession_t *session, const RAD_NetEndTurnRequest_t *request);

///
/// Der Ausschnitt (x, y) mit Breite "w" und Hoehe "h" (discover.c).
///
void RAD_IoNetRequestDiscover(RAD_IoNetSession_t *session, uint32_t x, uint32_t y, uint32_t w, uint32_t h);

///
/// Die Reserven aller Spieler (discover_reserve.c, protobuf/discover.proto). Die
/// Antwort kommt als eine Nachricht je Einheit (RAD_IoNetOnReserveUnit). Ohne
/// Session: fuer die Antwort ist nichts festzuhalten.
///
void RAD_IoNetRequestDiscoverReserve(void);

///
/// Alle Einheiten, die einem Spieler gehoeren (discover_units.c,
/// protobuf/discover.proto), wie die Reserven: eine Nachricht je Einheit
/// (RAD_IoNetOnUnit), ohne Session.
///
void RAD_IoNetRequestDiscoverUnits(void);

#endif
