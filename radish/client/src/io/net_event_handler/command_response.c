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

    // In die Welt traegt die Antwort nichts ein: sie wiederholt den angefragten
    // Weg, nicht den gegangenen. Einen ausgefuehrten Zug bringt vorher das
    // Ereignis (unit_moved.c).
}
