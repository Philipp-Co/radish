#include "common.h"

#include <radish/io/net_codec.h>
#include <radish/io/net_request.h>
#include <radish/io/net_transport.h>

#include <emscripten/emscripten.h>
#include <stdio.h>

bool RAD_IoNetRequestEndTurn(RAD_IoNetSession_t *session, const RAD_NetEndTurnRequest_t *request)
{
    uint8_t message[RAD_IO_NET_REQUEST_MESSAGE_MAX];

    size_t payload_length = 0;
    const RAD_NetCodecResult_t result = RAD_NetEncodeEndTurnRequest(
        request, message, sizeof(message), &payload_length);

    if(result != RAD_NET_CODEC_OK)
    {
        printf("Kommando nicht verschickt: %s\n", RAD_NetCodecResultText(result));
        return false;
    }

    RAD_IoNetTransmit(message, payload_length);

    session->last_send_time_ms = emscripten_get_now();
    session->awaiting_sequence = request->sequence;

    printf("-> #%llu Zug abgeben\n", (unsigned long long)request->sequence);
    return true;
}
