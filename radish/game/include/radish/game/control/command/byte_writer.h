#ifndef __RAD_BYTE_WRITER_H__
#define __RAD_BYTE_WRITER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

///
/// Schreibt Ganzzahlen als Bytefolge in einen Puffer des Aufrufers. Die
/// Serialisierer der Kommandos beschreiben damit nur ihre Felder und wissen
/// nichts von Byte-Reihenfolge und Platz -- dieselbe Trennung wie zwischen
/// RAD_JsonWriter_t und den JSON-Serialisierern.
///
/// Big Endian, weil es die Reihenfolge auf der Strecke ist: der Client schreibt
/// sein Codefeld schon so, und so steht das erste Byte einer Zahl auch im
/// Hexdump vorne.
///
/// Die Schreibfunktionen geben nichts zurueck. Ist der Puffer voll, merkt sich
/// der Writer das in "overflow" und ignoriert alles Weitere; geprueft wird
/// deshalb nicht nach jedem Feld, sondern einmal am Ende mit RAD_ByteWriterOk.
/// Ein Ueberlauf laesst den Puffer angeschrieben zurueck -- wer ihn weitergibt,
/// muss vorher fragen.
///
typedef struct
{
    uint8_t *buffer;
    size_t capacity;
    size_t length;

    bool overflow;
} RAD_ByteWriter_t;


void RAD_ByteWriterInit(RAD_ByteWriter_t *writer, uint8_t *buffer, size_t capacity);
bool RAD_ByteWriterOk(const RAD_ByteWriter_t *writer);

void RAD_ByteWriteUint8(RAD_ByteWriter_t *writer, uint8_t value);
void RAD_ByteWriteUint16(RAD_ByteWriter_t *writer, uint16_t value);
void RAD_ByteWriteUint32(RAD_ByteWriter_t *writer, uint32_t value);
void RAD_ByteWriteUint64(RAD_ByteWriter_t *writer, uint64_t value);

///
/// Die vorzeichenbehafteten Gegenstuecke schreiben das Zweierkomplement in
/// derselben Breite -- ein int16_t belegt also dieselben zwei Byte wie ein
/// uint16_t, und -1 steht als 0xFFFF auf der Strecke.
///
void RAD_ByteWriteInt8(RAD_ByteWriter_t *writer, int8_t value);
void RAD_ByteWriteInt16(RAD_ByteWriter_t *writer, int16_t value);
void RAD_ByteWriteInt32(RAD_ByteWriter_t *writer, int32_t value);

#endif
