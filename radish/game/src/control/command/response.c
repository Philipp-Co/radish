#include <radish/game/control/command/response.h>


void RAD_SerializeCommandResponse(RAD_ByteWriter_t *writer, const RAD_CommandResponse_t *response)
{
    RAD_SerializeCommandHeader(writer, &response->header);
    RAD_ByteWriteUint32(writer, response->value);
    RAD_SerializeCommand(writer, &response->command);
}

RAD_CommandCodecResult_t RAD_DeserializeCommandResponse(RAD_ByteReader_t *reader, RAD_CommandResponse_t *response)
{
    // Wie beim Kommando: in eine eigene Antwort lesen und "response" erst am Ende
    // setzen, damit es bei jedem Fehler unberuehrt bleibt.
    RAD_CommandResponse_t parsed = {0};

    RAD_CommandCodecResult_t result = RAD_DeserializeCommandHeader(reader, &parsed.header);
    if(result != RAD_COMMAND_CODEC_OK)
    {
        return result;
    }

    if(!RAD_ByteReadUint32(reader, &parsed.value))
    {
        return RAD_COMMAND_CODEC_ERROR_TRUNCATED;
    }

    // Der Rest der Nachricht ist eine ganze Kommandonachricht, und weil sie am Ende
    // steht, laesst sich der Einstiegspunkt des Kommandos unveraendert nehmen: er
    // prueft die Nutzlast und dass danach kein Byte uebrig bleibt -- deshalb steht
    // hier kein eigener Trailing-Check mehr.
    result = RAD_DeserializeCommand(reader, &parsed.command);
    if(result != RAD_COMMAND_CODEC_OK)
    {
        return result;
    }

    // Beide Koepfe muessen dasselbe tragen. Geprueft wird es hier und nicht
    // geglaubt: die Antwort kommt von der Gegenseite, und ein Absender, der den
    // Kopf zweimal ungleich schreibt, laesst offen, welcher der beiden gilt.
    if(parsed.command.header.type != parsed.header.type ||
       parsed.command.header.sequence != parsed.header.sequence ||
       parsed.command.header.user != parsed.header.user)
    {
        return RAD_COMMAND_CODEC_ERROR_HEADER_MISMATCH;
    }

    *response = parsed;

    return RAD_COMMAND_CODEC_OK;
}
