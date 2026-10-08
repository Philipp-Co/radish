#include "common.h"

#include <radish/io/net_codec.h>
#include <radish/io/net_request.h>
#include <radish/io/net_transport.h>

#include <stdio.h>

void RAD_IoNetRequestDiscoverUnits(void)
{
    uint8_t message[RAD_IO_NET_REQUEST_MESSAGE_MAX];

    size_t payload_length = 0;
    const RAD_NetCodecResult_t result = RAD_NetEncodeDiscoverUnits(message, sizeof(message), &payload_length);
    if(result != RAD_NET_CODEC_OK)
    {
        printf("Einheiten-Anfrage nicht verschickt: %s\n", RAD_NetCodecResultText(result));
        return;
    }

    RAD_IoNetTransmit(message, payload_length);
    printf("-> Einheiten-Anfrage\n");
}
