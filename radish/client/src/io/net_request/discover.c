#include "common.h"

#include <radish/io/net_codec.h>
#include <radish/io/net_request.h>
#include <radish/io/net_transport.h>

#include <stdio.h>

void RAD_IoNetRequestDiscover(RAD_IoNetSession_t *session, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    uint8_t message[RAD_IO_NET_REQUEST_MESSAGE_MAX];

    size_t payload_length = 0;
    const RAD_NetCodecResult_t result = RAD_NetEncodeDiscover(
        x, y, w, h, message, sizeof(message), &payload_length);

    if(result != RAD_NET_CODEC_OK)
    {
        printf("Discover nicht verschickt: %s\n", RAD_NetCodecResultText(result));
        return;
    }

    RAD_IoNetTransmit(message, payload_length);

    session->last_discover_request = (RAD_IoNetDiscoverRequest_t){ .sent = true, .x = x, .y = y, .w = w, .h = h };

    printf("-> Discover: Ausschnitt x=%u y=%u w=%u h=%u\n", (unsigned)x, (unsigned)y, (unsigned)w, (unsigned)h);
}
