#ifndef __RAD_BYTE_READER_H__
#define __RAD_BYTE_READER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

///
/// Laeuft als Cursor ueber eine Bytefolge und liest Ganzzahlen in der Reihenfolge
/// heraus, in der der Writer sie geschrieben hat -- Big Endian, dieselben Breiten.
/// Das Gegenstueck zu RAD_ByteWriter_t und, eine Ebene hoeher, zu
/// RAD_JsonReader_t.
///
/// Wie dort ist der Fehler klebrig: nach dem ersten Fehlschlag liefert jede
/// weitere Leseoperation false, ohne den Cursor weiterzuschieben. Ein
/// Deserialisierer kann seine Felder deshalb hintereinander lesen und erst danach
/// fragen, ob alles da war.
///
/// Der Reader besitzt den Puffer nicht und schreibt nie in ihn. Er darf ueber
/// eine empfangene Nachricht laufen, die von irgendwo kommt: mehr als
/// "size" Byte liest er nicht.
///
typedef struct
{
    const uint8_t *buffer;
    size_t size;
    size_t cursor;

    bool error;
} RAD_ByteReader_t;


void RAD_ByteReaderInit(RAD_ByteReader_t *reader, const uint8_t *buffer, size_t size);
bool RAD_ByteReaderOk(const RAD_ByteReader_t *reader);

///
/// Wie viele Byte noch nicht gelesen sind. Damit erkennt der Aufrufer eine
/// Nachricht, die laenger ist als das Kommando darin -- siehe
/// RAD_COMMAND_CODEC_ERROR_TRAILING_BYTES.
///
size_t RAD_ByteReaderRemaining(const RAD_ByteReader_t *reader);

bool RAD_ByteReadUint8(RAD_ByteReader_t *reader, uint8_t *value);
bool RAD_ByteReadUint16(RAD_ByteReader_t *reader, uint16_t *value);
bool RAD_ByteReadUint32(RAD_ByteReader_t *reader, uint32_t *value);
bool RAD_ByteReadUint64(RAD_ByteReader_t *reader, uint64_t *value);

bool RAD_ByteReadInt8(RAD_ByteReader_t *reader, int8_t *value);
bool RAD_ByteReadInt16(RAD_ByteReader_t *reader, int16_t *value);
bool RAD_ByteReadInt32(RAD_ByteReader_t *reader, int32_t *value);

#endif
