#include <radish/game/control/command/byte_reader.h>


void RAD_ByteReaderInit(RAD_ByteReader_t *reader, const uint8_t *buffer, size_t size)
{
    reader->buffer = buffer;
    reader->size = size;
    reader->cursor = 0;
    reader->error = false;
}

bool RAD_ByteReaderOk(const RAD_ByteReader_t *reader)
{
    return !reader->error;
}

size_t RAD_ByteReaderRemaining(const RAD_ByteReader_t *reader)
{
    return reader->size - reader->cursor;
}

///
/// Alle Lesefunktionen laufen hier zusammen. Ist der Puffer zu Ende, wird der
/// Fehler gesetzt; er ist klebrig, und der Cursor bleibt danach stehen.
///
static bool read_byte(RAD_ByteReader_t *reader, uint8_t *out)
{
    if(reader->error)
    {
        return false;
    }

    if(reader->cursor >= reader->size)
    {
        reader->error = true;
        return false;
    }

    *out = reader->buffer[reader->cursor];
    reader->cursor += 1;
    return true;
}

bool RAD_ByteReadUint8(RAD_ByteReader_t *reader, uint8_t *value)
{
    return read_byte(reader, value);
}

bool RAD_ByteReadUint16(RAD_ByteReader_t *reader, uint16_t *value)
{
    uint8_t high = 0;
    uint8_t low = 0;

    if(!read_byte(reader, &high) || !read_byte(reader, &low))
    {
        return false;
    }

    *value = (uint16_t)(((uint16_t)high << 8) | (uint16_t)low);
    return true;
}

bool RAD_ByteReadUint32(RAD_ByteReader_t *reader, uint32_t *value)
{
    uint32_t result = 0;

    for(int i = 0; i < 4; ++i)
    {
        uint8_t byte = 0;
        if(!read_byte(reader, &byte))
        {
            return false;
        }
        result = (result << 8) | (uint32_t)byte;
    }

    *value = result;
    return true;
}

bool RAD_ByteReadUint64(RAD_ByteReader_t *reader, uint64_t *value)
{
    uint64_t result = 0;

    for(int i = 0; i < 8; ++i)
    {
        uint8_t byte = 0;
        if(!read_byte(reader, &byte))
        {
            return false;
        }
        result = (result << 8) | (uint64_t)byte;
    }

    *value = result;
    return true;
}

///
/// Zurueck ins Vorzeichenbehaftete, und zwar auf dem langen Weg: eine Umwandlung
/// per Cast waere fuer Werte oberhalb von INT*_MAX nur implementierungsdefiniert
/// -- in der Praxis liefert jeder Compiler das Zweierkomplement, zugesichert ist
/// es aber erst in C23, und radish legt den Standard nicht fest. Die Rechnung
/// unten ist fuer jeden Wert definiert und liefert dasselbe.
///
static int8_t to_int8(uint8_t bits)
{
    return (bits <= INT8_MAX) ? (int8_t)bits : (int8_t)((int)bits - 256);
}

static int16_t to_int16(uint16_t bits)
{
    return (bits <= INT16_MAX) ? (int16_t)bits : (int16_t)((int32_t)bits - 65536);
}

static int32_t to_int32(uint32_t bits)
{
    return (bits <= INT32_MAX) ? (int32_t)bits : (int32_t)(bits - 2147483648u) + INT32_MIN;
}

bool RAD_ByteReadInt8(RAD_ByteReader_t *reader, int8_t *value)
{
    uint8_t bits = 0;
    if(!RAD_ByteReadUint8(reader, &bits))
    {
        return false;
    }

    *value = to_int8(bits);
    return true;
}

bool RAD_ByteReadInt16(RAD_ByteReader_t *reader, int16_t *value)
{
    uint16_t bits = 0;
    if(!RAD_ByteReadUint16(reader, &bits))
    {
        return false;
    }

    *value = to_int16(bits);
    return true;
}

bool RAD_ByteReadInt32(RAD_ByteReader_t *reader, int32_t *value)
{
    uint32_t bits = 0;
    if(!RAD_ByteReadUint32(reader, &bits))
    {
        return false;
    }

    *value = to_int32(bits);
    return true;
}
