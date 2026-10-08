#include <radish/control/attack_unit.h>
#include <radish/io/net_request.h>


bool RAD_ControlAttackUnit(RAD_IoNetSession_t *session, RAD_NetEntityId_t unit, int16_t x, int16_t y)
{
    const RAD_NetAttackRequest_t request = {
        .sequence = session->next_sequence++,
        .user = session->own_player_id,
        .entity = unit,
        .target = { .x = x, .y = y }
    };

    return RAD_IoNetRequestAttack(session, &request);
}
