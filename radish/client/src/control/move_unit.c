#include <radish/control/move_unit.h>
#include <radish/io/net_request.h>


bool RAD_ControlMoveUnit(RAD_IoNetSession_t *session, RAD_NetEntityId_t unit, const RAD_NetPath_t *path)
{
    const RAD_NetMoveRequest_t request = {
        .sequence = session->next_sequence++,
        .user = session->own_player_id,
        .entity = unit,
        .path = *path
    };

    return RAD_IoNetRequestMove(session, &request);
}
