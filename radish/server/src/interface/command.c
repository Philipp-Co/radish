#include <interface/command.h>

///
/// Der ganze Adapter: den Cursor des Codecs auf die Nachricht setzen und
/// weiterreichen. Mehr ist hier mit Absicht nicht -- was gelesen und geschrieben
/// wird, steht in codec.h und response.h, und zwar nur dort.
///

RAD_CommandCodecResult_t RAD_ParseCommandFromMessage(const uint8_t *message, uint16_t size, RAD_Command_t *out_command)
{
    RAD_ByteReader_t reader;
    RAD_ByteReaderInit(&reader, message, size);

    return RAD_DeserializeCommand(&reader, out_command);
}

RAD_CommandResponse_t RAD_CreateCommandResponse(const RAD_Command_t *command, uint32_t value)
{
    // Der Kopf wird als Ganzes uebernommen, nicht Feld fuer Feld: so kommt eine
    // spaetere Ergaenzung in RAD_CommandHeader_t von selbst mit, statt hier
    // vergessen zu werden.
    RAD_CommandResponse_t response = {
        .header = command->header,
        .value = value
    };

    return response;
}

bool RAD_SerializeCommandResponseToMessage(const RAD_CommandResponse_t *response, uint8_t *out_message, uint16_t capacity, uint16_t *out_size)
{
    RAD_ByteWriter_t writer;
    RAD_ByteWriterInit(&writer, out_message, capacity);

    RAD_SerializeCommandResponse(&writer, response);

    // Erst hier gefragt, nicht nach jedem Feld: der Ueberlauf klebt im Writer.
    // "out_size" bleibt bei einem Ueberlauf unangetastet, damit niemand die
    // angeschriebenen Bytes fuer eine Nachricht haelt.
    if(!RAD_ByteWriterOk(&writer))
    {
        return false;
    }

    *out_size = (uint16_t)writer.length;

    return true;
}
