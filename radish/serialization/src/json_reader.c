#include <radish/serialization/json_reader.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>

static const jsmntok_t* RAD_JsonCurrent(const RAD_JsonReader_t *reader);
static int32_t RAD_JsonSubtreeLength(const RAD_JsonReader_t *reader, int32_t index);
static bool RAD_JsonFail(RAD_JsonReader_t *reader);

void RAD_JsonReaderInit(RAD_JsonReader_t *reader, const char *json, const jsmntok_t *tokens, int32_t number_of_tokens)
{
    reader->json = json;
    reader->tokens = tokens;
    reader->number_of_tokens = number_of_tokens;
    reader->cursor = 0;
    reader->error = (json == NULL || tokens == NULL || number_of_tokens <= 0);
}

bool RAD_JsonReaderOk(const RAD_JsonReader_t *reader)
{
    return !reader->error;
}

bool RAD_JsonReadBeginObject(RAD_JsonReader_t *reader, int32_t *number_of_fields)
{
    const jsmntok_t *token = RAD_JsonCurrent(reader);
    if(token == NULL || token->type != JSMN_OBJECT)
    {
        return RAD_JsonFail(reader);
    }

    *number_of_fields = token->size;
    reader->cursor++;
    return true;
}

bool RAD_JsonReadBeginArray(RAD_JsonReader_t *reader, int32_t *number_of_elements)
{
    const jsmntok_t *token = RAD_JsonCurrent(reader);
    if(token == NULL || token->type != JSMN_ARRAY)
    {
        return RAD_JsonFail(reader);
    }

    *number_of_elements = token->size;
    reader->cursor++;
    return true;
}

bool RAD_JsonReadKey(RAD_JsonReader_t *reader, char *out, size_t size)
{
    const jsmntok_t *token = RAD_JsonCurrent(reader);

    // Ein Schluessel ist ein String, der genau einen Wert traegt (size == 1);
    // ein String als Wert hat size == 0.
    if(token == NULL || token->type != JSMN_STRING || token->size != 1)
    {
        return RAD_JsonFail(reader);
    }

    size_t length = (size_t)(token->end - token->start);
    if(length + 1 > size)
    {
        return RAD_JsonFail(reader);
    }

    memcpy(out, reader->json + token->start, length);
    out[length] = '\0';
    reader->cursor++;
    return true;
}

bool RAD_JsonReadString(RAD_JsonReader_t *reader, char *out, size_t size)
{
    const jsmntok_t *token = RAD_JsonCurrent(reader);
    if(token == NULL || token->type != JSMN_STRING || token->size != 0)
    {
        return RAD_JsonFail(reader);
    }

    size_t length = (size_t)(token->end - token->start);
    if(length + 1 > size)
    {
        return RAD_JsonFail(reader);
    }

    memcpy(out, reader->json + token->start, length);
    out[length] = '\0';
    reader->cursor++;
    return true;
}

bool RAD_JsonReadInt(RAD_JsonReader_t *reader, int32_t *value)
{
    const jsmntok_t *token = RAD_JsonCurrent(reader);
    if(token == NULL || token->type != JSMN_PRIMITIVE)
    {
        return RAD_JsonFail(reader);
    }

    char text[24];
    size_t length = (size_t)(token->end - token->start);
    if(length == 0 || length + 1 > sizeof(text))
    {
        return RAD_JsonFail(reader);
    }
    memcpy(text, reader->json + token->start, length);
    text[length] = '\0';

    char *end = NULL;
    errno = 0;
    long parsed = strtol(text, &end, 10);

    // Der gesamte Token muss aufgebraucht sein -- "12abc" oder "true" sind keine Zahl.
    if(end != text + length || errno == ERANGE || parsed < INT32_MIN || parsed > INT32_MAX)
    {
        return RAD_JsonFail(reader);
    }

    *value = (int32_t)parsed;
    reader->cursor++;
    return true;
}

bool RAD_JsonReadUInt64(RAD_JsonReader_t *reader, uint64_t *value)
{
    // 18 Zeichen schreibt der Writer, 20 laesst auch eine dezimale Angabe von
    // Hand durch (2^64-1 hat zwanzig Stellen).
    char text[21];
    if(!RAD_JsonReadString(reader, text, sizeof(text)))
    {
        return false;
    }

    // strtoull nimmt fuehrende Leerzeichen und ein Minus an und dreht den Wert
    // um; beides ist hier keine Angabe. Eine Ziffer am Anfang schliesst es aus,
    // das 0x-Praefix eingeschlossen.
    if(text[0] < '0' || text[0] > '9')
    {
        return RAD_JsonFail(reader);
    }

    char *end = NULL;
    errno = 0;

    // Basis 0: "0x..." wird hexadezimal gelesen, alles andere dezimal.
    unsigned long long parsed = strtoull(text, &end, 0);

    // Der gesamte String muss aufgebraucht sein -- "0x1f Rest" ist keine Zahl.
    if(*end != '\0' || errno == ERANGE)
    {
        return RAD_JsonFail(reader);
    }

    *value = (uint64_t)parsed;
    return true;
}

bool RAD_JsonPeekIsNull(const RAD_JsonReader_t *reader)
{
    const jsmntok_t *token = RAD_JsonCurrent(reader);
    return token != NULL
        && token->type == JSMN_PRIMITIVE
        && (token->end - token->start) == 4
        && memcmp(reader->json + token->start, "null", 4) == 0;
}

void RAD_JsonSkipValue(RAD_JsonReader_t *reader)
{
    if(reader->error)
    {
        return;
    }

    int32_t length = RAD_JsonSubtreeLength(reader, reader->cursor);
    if(length <= 0)
    {
        RAD_JsonFail(reader);
        return;
    }
    reader->cursor += length;
}

static const jsmntok_t* RAD_JsonCurrent(const RAD_JsonReader_t *reader)
{
    if(reader->error || reader->cursor < 0 || reader->cursor >= reader->number_of_tokens)
    {
        return NULL;
    }
    return &reader->tokens[reader->cursor];
}

static bool RAD_JsonFail(RAD_JsonReader_t *reader)
{
    reader->error = true;
    return false;
}

///
/// Anzahl der Tokens, die der Wert an dieser Stelle einnimmt. jsmn legt die
/// Tokens in Dokumentreihenfolge ab und vermerkt in "size" die Zahl der Felder
/// bzw. Elemente -- daraus laesst sich ein Teilbaum ohne Elternzeiger abzaehlen.
///
static int32_t RAD_JsonSubtreeLength(const RAD_JsonReader_t *reader, int32_t index)
{
    if(index < 0 || index >= reader->number_of_tokens)
    {
        return 0;
    }

    const jsmntok_t *token = &reader->tokens[index];
    int32_t length = 1;

    if(token->type == JSMN_OBJECT)
    {
        for(int32_t i=0;i < token->size; ++i)
        {
            length += 1;    // Schluessel
            int32_t value_length = RAD_JsonSubtreeLength(reader, index + length);
            if(value_length <= 0)
            {
                return 0;
            }
            length += value_length;
        }
    }
    else if(token->type == JSMN_ARRAY)
    {
        for(int32_t i=0;i < token->size; ++i)
        {
            int32_t element_length = RAD_JsonSubtreeLength(reader, index + length);
            if(element_length <= 0)
            {
                return 0;
            }
            length += element_length;
        }
    }

    return length;
}
