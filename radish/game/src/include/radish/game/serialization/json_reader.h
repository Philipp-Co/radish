#ifndef __RAD_JSON_READER_H__
#define __RAD_JSON_READER_H__

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <radish/game/serialization/serialization.h>

// Nur die Deklarationen; die Implementierung erzeugt einmalig jsmn_impl.c.
#define JSMN_HEADER
#include <jsmn.h>

#define RAD_JSON_KEY_MAX 32
#define RAD_JSON_NAME_MAX 32

///
/// Laeuft als Cursor ueber die flache Token-Liste, die jsmn in Dokument-
/// reihenfolge liefert. Der Fehler ist klebrig: nach dem ersten
/// Fehlschlag liefert jede weitere Leseoperation false, geprueft werden muss
/// also nicht zwingend jeder einzelne Schritt.
///
/// Der Reader entpackt keine Maskierungen in Strings -- fuer die Enum-Namen und
/// Schluessel dieses Formats reicht der rohe Ausschnitt.
///
typedef struct
{
    const char *json;
    const jsmntok_t *tokens;
    int32_t number_of_tokens;

    int32_t cursor;
    bool error;
} RAD_JsonReader_t;


void RAD_JsonReaderInit(RAD_JsonReader_t *reader, const char *json, const jsmntok_t *tokens, int32_t number_of_tokens);
bool RAD_JsonReaderOk(const RAD_JsonReader_t *reader);

bool RAD_JsonReadBeginObject(RAD_JsonReader_t *reader, int32_t *number_of_fields);
bool RAD_JsonReadBeginArray(RAD_JsonReader_t *reader, int32_t *number_of_elements);

bool RAD_JsonReadKey(RAD_JsonReader_t *reader, char *out, size_t size);
bool RAD_JsonReadInt(RAD_JsonReader_t *reader, int32_t *value);
bool RAD_JsonReadString(RAD_JsonReader_t *reader, char *out, size_t size);

///
/// Wie lang der String an der Cursorposition ist, ohne ihn zu lesen. false, wenn
/// dort kein String steht -- der Reader bleibt dabei in Ordnung.
///
/// Fuer Leser, bei denen die Laenge selbst eine Angabe ist: RAD_JsonReadString
/// scheitert an einem zu langen String genauso wie an einer Zahl, und wer dafuer
/// einen eigenen Fehler melden will, fragt vorher hier (world_definition.c).
///
bool RAD_JsonPeekStringLength(const RAD_JsonReader_t *reader, int32_t *length);

///
/// Ueberspringt den kompletten Wert an der Cursorposition samt allem, was darin
/// verschachtelt ist. Damit kann ein Leser einen Wert erst vermerken und spaeter
/// lesen -- etwa die Raster einer Weltdefinition erst, wenn ihre Version stimmt
/// (world_definition.c).
///
void RAD_JsonSkipValue(RAD_JsonReader_t *reader);

///
/// Zerlegt "json" in Tokens, fuer die Leser dieses Moduls -- bislang die
/// Weltdefinition (world_definition.c). Zweimal jsmn --
/// der erste Lauf zaehlt nur --, damit genau so viel belegt wird wie noetig, ohne
/// feste Obergrenze. Bei RAD_SERIALIZE_OK gehoert "*tokens" dem Aufrufer und wird
/// mit free() freigegeben; sonst ist es NULL.
///
/// Steht hier und nicht in serialization.h, weil es jsmntok_t nennt: wer nur das
/// Ergebnis braucht, soll jsmn nicht im Include-Pfad brauchen.
///
RAD_SerializeResult_t RAD_JsonTokenize(const char *json, size_t length, jsmntok_t **tokens, int32_t *number_of_tokens);

#endif
