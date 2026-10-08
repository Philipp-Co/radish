#ifndef __RAD_CONTROL_END_TURN_H__
#define __RAD_CONTROL_END_TURN_H__

#include <stdbool.h>
#include <radish/io/net_session.h>

///
/// Den eigenen Zug abgeben.
///
/// **Der Client schickt nur das Kommando**, wie beim Deployen
/// (control/deploy_unit.h): ob der Zug wirklich weitergeht, entscheidet der
/// Server -- abgeben kann nur, wer dran ist. Wer danach dran ist, erfaehrt der
/// Client ueber das Ereignis des Servers (io/net_event_handler/current_player.c)
/// und nicht hier.
///

///
/// Schickt das Kommando, den Zug abzugeben. Sequenznummer und Absender wie bei
/// RAD_ControlDeployUnit aus "session".
///
/// false, wenn sich das Kommando nicht packen liess; dann ist nichts
/// verschickt (RAD_IoNetRequestEndTurn), die Sequenznummer aber verbraucht.
///
bool RAD_ControlEndTurn(RAD_IoNetSession_t *session);

#endif
