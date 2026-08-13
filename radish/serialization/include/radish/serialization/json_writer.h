#ifndef __RAD_JSON_WRITER_H__
#define __RAD_JSON_WRITER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

///
/// Schreibt JSON in einen vom Aufrufer gestellten Puffer. Kommas, Einrueckung
/// und Verschachtelungstiefe verwaltet der Writer -- die Serializer beschreiben
/// nur die Struktur und wissen nichts von der Formatierung.
///
/// Alle Schreibfunktionen geben nichts zurueck: laeuft der Puffer ueber, merkt
/// sich der Writer das in "overflow" und ignoriert alles Weitere. Geprueft wird
/// deshalb nicht nach jedem Feld, sondern einmal am Ende mit RAD_JsonWriterOk.
/// Der Puffer ist zu jedem Zeitpunkt null-terminiert.
///
typedef struct
{
    char *buffer;
    size_t capacity;
    size_t length;

    int32_t depth;
    bool needs_comma;
    bool after_key;

    bool pretty;
    bool overflow;
} RAD_JsonWriter_t;


void RAD_JsonWriterInit(RAD_JsonWriter_t *writer, char *buffer, size_t capacity, bool pretty);
bool RAD_JsonWriterOk(const RAD_JsonWriter_t *writer);

void RAD_JsonWriteBeginObject(RAD_JsonWriter_t *writer);
void RAD_JsonWriteEndObject(RAD_JsonWriter_t *writer);
void RAD_JsonWriteBeginArray(RAD_JsonWriter_t *writer);
void RAD_JsonWriteEndArray(RAD_JsonWriter_t *writer);

void RAD_JsonWriteKey(RAD_JsonWriter_t *writer, const char *key);
void RAD_JsonWriteInt(RAD_JsonWriter_t *writer, int32_t value);
void RAD_JsonWriteString(RAD_JsonWriter_t *writer, const char *value);
void RAD_JsonWriteNull(RAD_JsonWriter_t *writer);

#endif
