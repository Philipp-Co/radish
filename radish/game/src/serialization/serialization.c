#include <radish/game/serialization/serialization.h>
#include <radish/game/serialization/json_reader.h>
#include <stdlib.h>

RAD_SerializeResult_t RAD_JsonTokenize(const char *json, size_t length, jsmntok_t **tokens, int32_t *number_of_tokens)
{
    *tokens = NULL;
    *number_of_tokens = 0;

    // Erster Lauf ohne Token-Array: jsmn zaehlt nur, wie viele es werden.
    jsmn_parser parser;
    jsmn_init(&parser);
    int counted = jsmn_parse(&parser, json, length, NULL, 0);
    if(counted < 1)
    {
        return RAD_SERIALIZE_ERROR_SYNTAX;
    }

    jsmntok_t *allocated = malloc((size_t)counted * sizeof(jsmntok_t));
    if(allocated == NULL)
    {
        return RAD_SERIALIZE_ERROR_OUT_OF_MEMORY;
    }

    jsmn_init(&parser);
    int parsed = jsmn_parse(&parser, json, length, allocated, (unsigned int)counted);
    if(parsed < 1)
    {
        free(allocated);
        return RAD_SERIALIZE_ERROR_SYNTAX;
    }

    *tokens = allocated;
    *number_of_tokens = parsed;
    return RAD_SERIALIZE_OK;
}

const char* RAD_SerializeResultText(RAD_SerializeResult_t result)
{
    switch(result)
    {
        case RAD_SERIALIZE_OK:                        return "in Ordnung";
        case RAD_SERIALIZE_ERROR_SYNTAX:              return "kein gueltiges JSON";
        case RAD_SERIALIZE_ERROR_SCHEMA:              return "unerwartete Struktur";
        case RAD_SERIALIZE_ERROR_VERSION:             return "nicht unterstuetzte Version";
        case RAD_SERIALIZE_ERROR_SIZE_MISMATCH:       return "Weltgroesse passt nicht";
        case RAD_SERIALIZE_ERROR_TILE_TYPE:           return "unbekannter Tile-Typ";
        case RAD_SERIALIZE_ERROR_INCONSISTENT:        return "Datei widerspricht sich selbst";
        case RAD_SERIALIZE_ERROR_WORLD_OCCUPIED:      return "auf der Welt stehen schon Figuren";
        case RAD_SERIALIZE_ERROR_OUT_OF_MEMORY:       return "kein Speicher";
        default:                                      return "unbekannter Fehler";
    }
}
