#include <radish/io/net_event_handler.h>

#include <emscripten/emscripten.h>
#include <stdio.h>

void RAD_IoNetOnAttackResponse(void *user_argument, const RAD_NetAttackResponse_t *response)
{
    RAD_IoNetEventHandlerContext_t *context = (RAD_IoNetEventHandlerContext_t*)user_argument;

    // Wie beim Deployment (deploy_response.c): die Zeit nur, wenn es die Antwort
    // auf das zuletzt verschickte eigene Kommando ist. Abgelehnt mit dem Grund
    // des Servers.
    const char *outcome = response->success ? "angenommen" : "abgelehnt";
    const char *reason = response->success ? "" : response->description;
    if(response->sequence == context->session->awaiting_sequence)
    {
        printf("<- #%llu Attack %d auf (%d, %d): %s %s (%.1f ms)\n",
               (unsigned long long)response->sequence, (int)response->entity,
               (int)response->target.x, (int)response->target.y, outcome, reason,
               emscripten_get_now() - context->session->last_send_time_ms);
    }
    else
    {
        printf("<- #%llu Attack %d auf (%d, %d): %s %s\n",
               (unsigned long long)response->sequence, (int)response->entity,
               (int)response->target.x, (int)response->target.y, outcome, reason);
    }

    // In die Welt traegt die Antwort nichts ein: einen Kampf, der etwas
    // aendern koennte, gibt es im Spiel noch nicht.
}
