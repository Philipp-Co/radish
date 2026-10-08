#include <radish/io/net_event_handler.h>

#include <emscripten/emscripten.h>
#include <stdio.h>

void RAD_IoNetOnDeployResponse(void *user_argument, const RAD_NetDeployResponse_t *response)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    // Wie beim Zug (command_response.c): die Zeit nur, wenn es die Antwort auf
    // das zuletzt verschickte eigene Kommando ist.
    if(response->sequence == context->session->awaiting_sequence)
    {
        printf("<- #%llu Deploy %d auf (%d, %d): %s (%.1f ms)\n",
               (unsigned long long)response->sequence, (int)response->entity,
               (int)response->position.x, (int)response->position.y,
               response->success ? "ausgefuehrt" : "abgelehnt",
               emscripten_get_now() - context->session->last_send_time_ms);
    }
    else
    {
        printf("<- #%llu Deploy %d auf (%d, %d): %s\n",
               (unsigned long long)response->sequence, (int)response->entity,
               (int)response->position.x, (int)response->position.y,
               response->success ? "ausgefuehrt" : "abgelehnt");
    }

    // In die Welt traegt die Antwort nichts mehr ein: ein ausgefuehrtes
    // Deployment kommt vorher schon als eigenes Ereignis (unit_deployed.c).
}
