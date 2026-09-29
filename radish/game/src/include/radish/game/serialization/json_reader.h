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
/// reihenfolge liefert. Wie beim Writer ist der Fehler klebrig: nach dem ersten
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
/// Liest, was RAD_JsonWriteUInt64 geschrieben hat: einen String, keine Zahl --
/// die Begruendung steht dort.
///
/// Hexadezimal mit 0x-Praefix, wie geschrieben; eine dezimale Angabe wird
/// genauso angenommen, damit ein von Hand geaenderter Spielstand nicht daran
/// scheitert. Ein Vorzeichen ist keine Angabe: "-1" wird abgelehnt, statt als
/// groesstmoeglicher Wert durchzugehen.
///
bool RAD_JsonReadUInt64(RAD_JsonReader_t *reader, uint64_t *value);

bool RAD_JsonPeekIsNull(const RAD_JsonReader_t *reader);

///
/// Wie lang der String an der Cursorposition ist, ohne ihn zu lesen. false, wenn
/// dort kein String steht -- der Reader bleibt dabei in Ordnung, wie beim Blick
/// auf null.
///
/// Fuer Leser, bei denen die Laenge selbst eine Angabe ist: RAD_JsonReadString
/// scheitert an einem zu langen String genauso wie an einer Zahl, und wer dafuer
/// einen eigenen Fehler melden will, fragt vorher hier (world_definition.c).
///
bool RAD_JsonPeekStringLength(const RAD_JsonReader_t *reader, int32_t *length);

///
/// Ueberspringt den kompletten Wert an der Cursorposition samt allem, was darin
/// verschachtelt ist. Damit koennen Deserialisierer unbekannte Schluessel
/// stillschweigend uebergehen, statt an ihnen zu scheitern -- das ist der
/// Mechanismus, ueber den aeltere Staende ein spaeter hinzugefuegtes Feld
/// ueberleben.
///
void RAD_JsonSkipValue(RAD_JsonReader_t *reader);

///
/// Zerlegt "json" in Tokens, fuer alle Leser dieses Moduls: den Spielstand
/// (serialization.c) und die Weltdefinition (world_definition.c). Zweimal jsmn --
/// der erste Lauf zaehlt nur --, damit genau so viel belegt wird wie noetig, ohne
/// feste Obergrenze. Bei RAD_SERIALIZE_OK gehoert "*tokens" dem Aufrufer und wird
/// mit free() freigegeben; sonst ist es NULL.
///
/// Steht hier und nicht in serialization.h, weil es jsmntok_t nennt: wer nur
/// speichern und laden will, soll jsmn nicht im Include-Pfad brauchen.
///
RAD_SerializeResult_t RAD_JsonTokenize(const char *json, size_t length, jsmntok_t **tokens, int32_t *number_of_tokens);

#endif
