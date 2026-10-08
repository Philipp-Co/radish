#include <radish/control/deploy_unit.h>
#include <radish/io/net_request.h>


bool RAD_ControlDeployUnit(RAD_IoNetSession_t *session, RAD_NetEntityId_t unit, int16_t x, int16_t y)
{
    const RAD_NetDeployRequest_t request = {
        .sequence = session->next_sequence++,
        .user = session->own_player_id,
        .entity = unit,
        .position = { .x = x, .y = y }
    };

    return RAD_IoNetRequestDeploy(session, &request);
}
