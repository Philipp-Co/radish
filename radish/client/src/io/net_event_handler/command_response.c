#include <radish/io/net_event_handler.h>

#include <emscripten/emscripten.h>
#include <stdio.h>

void RAD_IoNetOnCommandResponse(void *user_argument, const RAD_NetCommandResponse_t *response)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    if(response->sequence == context->session->awaiting_sequence)
    {
        printf("<- #%llu Antwort: %s (%.1f ms)\n",
               (unsigned long long)response->sequence,
               response->success ? "ausgefuehrt" : "abgelehnt",
               emscripten_get_now() - context->session->last_send_time_ms);
    }
    else
    {
        printf("<- #%llu Antwort: %s\n",
               (unsigned long long)response->sequence,
               response->success ? "ausgefuehrt" : "abgelehnt");
    }

    // Der Server schickt die geaenderten Felder nicht als Tile-Ereignis mit --
    // einen bestaetigten Zug traegt der Client deshalb selbst nach, bevor der
    // User-Input davon erfaehrt.
    if(response->success)
    {
        RAD_ClientWorldMoveEntity(context->world, response->entity, &response->path);
    }

    RAD_IoUserInputOnCommandResponseReceived(context->user_input, response);
}
