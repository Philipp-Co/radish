#include <radish/serialization/json_writer.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#define RAD_JSON_INDENT_WIDTH 2

static void RAD_JsonAppend(RAD_JsonWriter_t *writer, const char *text);
static void RAD_JsonAppendChar(RAD_JsonWriter_t *writer, char character);
static void RAD_JsonAppendEscaped(RAD_JsonWriter_t *writer, const char *text);
static void RAD_JsonAppendIndent(RAD_JsonWriter_t *writer);
static void RAD_JsonBeginValue(RAD_JsonWriter_t *writer);

void RAD_JsonWriterInit(RAD_JsonWriter_t *writer, char *buffer, size_t capacity, bool pretty)
{
    writer->buffer = buffer;
    writer->capacity = capacity;
    writer->length = 0;
    writer->depth = 0;
    writer->needs_comma = false;
    writer->after_key = false;
    writer->pretty = pretty;

    // Ohne Platz fuer die abschliessende Null ist der Puffer von vornherein zu klein.
    writer->overflow = (buffer == NULL || capacity == 0);
    if(!writer->overflow)
    {
        writer->buffer[0] = '\0';
    }
}

bool RAD_JsonWriterOk(const RAD_JsonWriter_t *writer)
{
    return !writer->overflow;
}

void RAD_JsonWriteBeginObject(RAD_JsonWriter_t *writer)
{
    RAD_JsonBeginValue(writer);
    RAD_JsonAppendChar(writer, '{');
    writer->depth++;
    writer->needs_comma = false;
}

void RAD_JsonWriteEndObject(RAD_JsonWriter_t *writer)
{
    // needs_comma ist genau dann gesetzt, wenn das Objekt mindestens ein Feld
    // hat -- ein leeres Objekt bleibt so "{}" statt "{\n}".
    bool had_members = writer->needs_comma;

    writer->depth--;
    if(writer->pretty && had_members)
    {
        RAD_JsonAppendChar(writer, '\n');
        RAD_JsonAppendIndent(writer);
    }
    RAD_JsonAppendChar(writer, '}');

    writer->needs_comma = true;
    writer->after_key = false;
}

void RAD_JsonWriteBeginArray(RAD_JsonWriter_t *writer)
{
    RAD_JsonBeginValue(writer);
    RAD_JsonAppendChar(writer, '[');
    writer->depth++;
    writer->needs_comma = false;
}

void RAD_JsonWriteEndArray(RAD_JsonWriter_t *writer)
{
    bool had_elements = writer->needs_comma;

    writer->depth--;
    if(writer->pretty && had_elements)
    {
        RAD_JsonAppendChar(writer, '\n');
        RAD_JsonAppendIndent(writer);
    }
    RAD_JsonAppendChar(writer, ']');

    writer->needs_comma = true;
    writer->after_key = false;
}

void RAD_JsonWriteKey(RAD_JsonWriter_t *writer, const char *key)
{
    RAD_JsonBeginValue(writer);
    RAD_JsonAppendChar(writer, '"');
    RAD_JsonAppendEscaped(writer, key);
    RAD_JsonAppendChar(writer, '"');
    RAD_JsonAppendChar(writer, ':');
    if(writer->pretty)
    {
        RAD_JsonAppendChar(writer, ' ');
    }

    writer->needs_comma = false;
    writer->after_key = true;
}

void RAD_JsonWriteInt(RAD_JsonWriter_t *writer, int32_t value)
{
    char text[16];
    snprintf(text, sizeof(text), "%" PRId32, value);

    RAD_JsonBeginValue(writer);
    RAD_JsonAppend(writer, text);
    writer->needs_comma = true;
}

void RAD_JsonWriteString(RAD_JsonWriter_t *writer, const char *value)
{
    RAD_JsonBeginValue(writer);
    RAD_JsonAppendChar(writer, '"');
    RAD_JsonAppendEscaped(writer, value);
    RAD_JsonAppendChar(writer, '"');
    writer->needs_comma = true;
}

void RAD_JsonWriteNull(RAD_JsonWriter_t *writer)
{
    RAD_JsonBeginValue(writer);
    RAD_JsonAppend(writer, "null");
    writer->needs_comma = true;
}

///
/// Trennung vor einem Wert, einem Schluessel oder einem oeffnenden Klammerpaar.
/// Folgt der Wert direkt auf seinen Schluessel, darf weder ein Komma noch ein
/// Umbruch dazwischen -- deshalb after_key.
///
static void RAD_JsonBeginValue(RAD_JsonWriter_t *writer)
{
    if(writer->after_key)
    {
        writer->after_key = false;
        return;
    }

    if(writer->needs_comma)
    {
        RAD_JsonAppendChar(writer, ',');
    }
    if(writer->pretty && writer->depth > 0)
    {
        RAD_JsonAppendChar(writer, '\n');
        RAD_JsonAppendIndent(writer);
    }
}

static void RAD_JsonAppendIndent(RAD_JsonWriter_t *writer)
{
    for(int32_t i=0;i < writer->depth * RAD_JSON_INDENT_WIDTH; ++i)
    {
        RAD_JsonAppendChar(writer, ' ');
    }
}

static void RAD_JsonAppend(RAD_JsonWriter_t *writer, const char *text)
{
    if(writer->overflow)
    {
        return;
    }

    size_t text_length = strlen(text);
    if(writer->length + text_length + 1 > writer->capacity)
    {
        writer->overflow = true;
        return;
    }

    memcpy(writer->buffer + writer->length, text, text_length);
    writer->length += text_length;
    writer->buffer[writer->length] = '\0';
}

static void RAD_JsonAppendChar(RAD_JsonWriter_t *writer, char character)
{
    char text[2] = { character, '\0' };
    RAD_JsonAppend(writer, text);
}

///
/// Unsere Strings sind ausschliesslich feste Schluessel und Enum-Namen, brauchen
/// also nie eine Maskierung. Sie trotzdem korrekt zu behandeln kostet wenig und
/// verhindert, dass die Funktion zur Falle wird, sobald hier einmal ein frei
/// waehlbarer Name durchlaeuft.
///
static void RAD_JsonAppendEscaped(RAD_JsonWriter_t *writer, const char *text)
{
    for(const char *cursor = text; *cursor != '\0'; ++cursor)
    {
        unsigned char character = (unsigned char)*cursor;
        switch(character)
        {
            case '"':  RAD_JsonAppend(writer, "\\\""); break;
            case '\\': RAD_JsonAppend(writer, "\\\\"); break;
            case '\b': RAD_JsonAppend(writer, "\\b");  break;
            case '\f': RAD_JsonAppend(writer, "\\f");  break;
            case '\n': RAD_JsonAppend(writer, "\\n");  break;
            case '\r': RAD_JsonAppend(writer, "\\r");  break;
            case '\t': RAD_JsonAppend(writer, "\\t");  break;
            default:
                if(character < 0x20)
                {
                    char escaped[7];
                    snprintf(escaped, sizeof(escaped), "\\u%04x", character);
                    RAD_JsonAppend(writer, escaped);
                }
                else
                {
                    RAD_JsonAppendChar(writer, (char)character);
                }
                break;
        }
    }
}
