#include <radish/game/control/command/byte_writer.h>


void RAD_ByteWriterInit(RAD_ByteWriter_t *writer, uint8_t *buffer, size_t capacity)
{
    writer->buffer = buffer;
    writer->capacity = capacity;
    writer->length = 0;
    writer->overflow = false;
}

bool RAD_ByteWriterOk(const RAD_ByteWriter_t *writer)
{
    return !writer->overflow;
}

///
/// Alle Schreibfunktionen laufen hier zusammen: ein Byte, oder der Ueberlauf ist
/// gesetzt. Der Ueberlauf ist klebrig -- ist er einmal gesetzt, wird nichts mehr
/// geschrieben, auch wenn ein spaeterer, kleinerer Wert noch passen wuerde. Sonst
/// entstuende eine Nachricht mit einem Loch in der Mitte, die sich lesen laesst
/// und trotzdem falsch ist.
///
static void write_byte(RAD_ByteWriter_t *writer, uint8_t value)
{
    if(writer->overflow)
    {
        return;
    }

    if(writer->length >= writer->capacity)
    {
        writer->overflow = true;
        return;
    }

    writer->buffer[writer->length] = value;
    writer->length += 1;
}

void RAD_ByteWriteUint8(RAD_ByteWriter_t *writer, uint8_t value)
{
    write_byte(writer, value);
}

void RAD_ByteWriteUint16(RAD_ByteWriter_t *writer, uint16_t value)
{
    write_byte(writer, (uint8_t)(value >> 8));
    write_byte(writer, (uint8_t)value);
}

void RAD_ByteWriteUint32(RAD_ByteWriter_t *writer, uint32_t value)
{
    write_byte(writer, (uint8_t)(value >> 24));
    write_byte(writer, (uint8_t)(value >> 16));
    write_byte(writer, (uint8_t)(value >> 8));
    write_byte(writer, (uint8_t)value);
}

void RAD_ByteWriteUint64(RAD_ByteWriter_t *writer, uint64_t value)
{
    for(int shift = 56; shift >= 0; shift -= 8)
    {
        write_byte(writer, (uint8_t)(value >> shift));
    }
}

///
/// Die Umwandlung ins Vorzeichenlose ist hier der eigentliche Vorgang: sie ist in
/// C fuer jeden Wert definiert und liefert genau das Zweierkomplement (der Wert
/// wird modulo 2^n genommen). Umgekehrt braucht der Reader dafuer einen Schritt
/// mehr, siehe byte_reader.c.
///
void RAD_ByteWriteInt8(RAD_ByteWriter_t *writer, int8_t value)
{
    RAD_ByteWriteUint8(writer, (uint8_t)value);
}

void RAD_ByteWriteInt16(RAD_ByteWriter_t *writer, int16_t value)
{
    RAD_ByteWriteUint16(writer, (uint16_t)value);
}

void RAD_ByteWriteInt32(RAD_ByteWriter_t *writer, int32_t value)
{
    RAD_ByteWriteUint32(writer, (uint32_t)value);
}
