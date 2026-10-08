#include <radish/control/end_turn.h>
#include <radish/io/net_request.h>


bool RAD_ControlEndTurn(RAD_IoNetSession_t *session)
{
    const RAD_NetEndTurnRequest_t request = {
        .sequence = session->next_sequence++,
        .user = session->own_player_id
    };

    return RAD_IoNetRequestEndTurn(session, &request);
}
