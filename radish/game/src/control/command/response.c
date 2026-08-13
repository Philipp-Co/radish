#include <radish/game/control/command/response.h>


void RAD_SerializeCommandResponse(RAD_ByteWriter_t *writer, const RAD_CommandResponse_t *response)
{
    RAD_SerializeCommandHeader(writer, &response->header);
    RAD_ByteWriteUint32(writer, response->value);
}

RAD_CommandCodecResult_t RAD_DeserializeCommandResponse(RAD_ByteReader_t *reader, RAD_CommandResponse_t *response)
{
    // Wie beim Kommando: in eine eigene Antwort lesen und "response" erst am Ende
    // setzen, damit es bei jedem Fehler unberuehrt bleibt.
    RAD_CommandResponse_t parsed = {0};

    const RAD_CommandCodecResult_t result = RAD_DeserializeCommandHeader(reader, &parsed.header);
    if(result != RAD_COMMAND_CODEC_OK)
    {
        return result;
    }

    if(!RAD_ByteReadUint32(reader, &parsed.value))
    {
        return RAD_COMMAND_CODEC_ERROR_TRUNCATED;
    }

    if(RAD_ByteReaderRemaining(reader) != 0)
    {
        return RAD_COMMAND_CODEC_ERROR_TRAILING_BYTES;
    }

    *response = parsed;

    return RAD_COMMAND_CODEC_OK;
}
