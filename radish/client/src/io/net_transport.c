#include <radish/io/net_transport.h>

#include <emscripten/emscripten.h>

EM_JS(void, zuc_js_send, (const uint8_t *data, int length), {
    if(Module.sendToChannel)
    {
        Module.sendToChannel(HEAPU8.slice(data, data + length));
    }
});

void RAD_IoNetTransmit(const uint8_t *data, size_t length)
{
    zuc_js_send(data, (int)length);
}
