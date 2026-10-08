#include <radish/io/net_event_handler.h>

#include <emscripten/emscripten.h>
#include <stdio.h>

void RAD_IoNetOnEndTurnResponse(void *user_argument, const RAD_NetEndTurnResponse_t *response)
{
    const RAD_IoNetEventHandlerContext_t *context = (const RAD_IoNetEventHandlerContext_t*)user_argument;

    // Wie beim Deployment (deploy_response.c): die Zeit nur, wenn es die Antwort
    // auf das zuletzt verschickte eigene Kommando ist.
    if(response->sequence == context->session->awaiting_sequence)
    {
        printf("<- #%llu Abgeben von 0x%llx: %s (%.1f ms)\n",
               (unsigned long long)response->sequence, (unsigned long long)response->user,
               response->success ? "ausgefuehrt" : "abgelehnt",
               emscripten_get_now() - context->session->last_send_time_ms);
    }
    else
    {
        printf("<- #%llu Abgeben von 0x%llx: %s\n",
               (unsigned long long)response->sequence, (unsigned long long)response->user,
               response->success ? "ausgefuehrt" : "abgelehnt");
    }

    // In die Welt traegt die Antwort nichts ein: wer danach dran ist, kommt
    // vorher schon als eigenes Ereignis (current_player.c).
}
