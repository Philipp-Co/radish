#include <radish/game/serialization/serialization.h>
#include <radish/game/serialization/game_serializer.h>
#include <radish/game/serialization/json_writer.h>
#include <radish/game/serialization/json_reader.h>
#include <stdlib.h>
#include <string.h>

static RAD_SerializeResult_t RAD_DeserializeRoot(RAD_JsonReader_t *reader, RAD_Game_t *game);

RAD_SerializeResult_t RAD_SerializeGameToJson(const RAD_Game_t *game, char *buffer, size_t capacity, bool pretty, size_t *written)
{
    RAD_JsonWriter_t writer;
    RAD_JsonWriterInit(&writer, buffer, capacity, pretty);

    RAD_JsonWriteBeginObject(&writer);

    RAD_JsonWriteKey(&writer, "format");
    RAD_JsonWriteString(&writer, RAD_SAVE_FORMAT_NAME);

    RAD_JsonWriteKey(&writer, "version");
    RAD_JsonWriteInt(&writer, RAD_SAVE_FORMAT_VERSION);

    RAD_JsonWriteKey(&writer, "game");
    RAD_SerializeGame(&writer, game);

    RAD_JsonWriteEndObject(&writer);

    if(!RAD_JsonWriterOk(&writer))
    {
        return RAD_SERIALIZE_ERROR_BUFFER_TOO_SMALL;
    }

    if(written != NULL)
    {
        *written = writer.length;
    }
    return RAD_SERIALIZE_OK;
}

RAD_SerializeResult_t RAD_DeserializeGameFromJson(RAD_Game_t *game, const char *json, size_t length)
{
    if(game == NULL || json == NULL)
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    jsmntok_t *tokens = NULL;
    int32_t parsed = 0;
    RAD_SerializeResult_t tokenized = RAD_JsonTokenize(json, length, &tokens, &parsed);
    if(tokenized != RAD_SERIALIZE_OK)
    {
        return tokenized;
    }

    // In ein eigenes Spiel lesen und erst bei Erfolg uebernehmen, damit ein
    // fehlgeschlagener Ladevorgang das laufende Spiel nicht halb ueberschreibt.
    RAD_Game_t *scratch = malloc(sizeof(RAD_Game_t));
    if(scratch == NULL)
    {
        free(tokens);
        return RAD_SERIALIZE_ERROR_OUT_OF_MEMORY;
    }

    // Der Zwischenstand faengt als Abzug des Ziels an, nicht bei frisch belegtem
    // Speicher: alles, was nicht im Spielstand steht, gehoert zum laufenden
    // Programm und muss die Uebernahme "*game = *scratch" unveraendert
    // ueberstehen. Das ist die Ereignisverwaltung -- sonst liest der Aufbau der
    // Welt einen Zeiger, der nirgendwo hinzeigt --, und genauso local_user, die
    // Sequenznummer und die Liste der ausgefuehrten Kommandos: gespeichert wird
    // davon nichts, und aus malloc kaeme dafuer Muell.
    //
    // Dazu gehoert auch der Zug samt seiner Reihe. Wer mitspielt, ist eine Frage
    // der Sitzung und keine des Spielstands: die Benutzer haengen an einem
    // laufenden Programm, ein Ladevorgang darf sie nicht hinauswerfen. Der Besitz
    // einer Figur steht dagegen sehr wohl in der Datei -- er haengt an der Uuid
    // und nicht an der Verbindung und findet seinen Benutzer wieder, sobald der
    // wieder mitspielt.
    //
    // Ein RAD_InitWorld() steht hier bewusst nicht: RAD_DeserializeWorld()
    // setzt die Welt selbst zurueck, bevor es sie fuellt -- und zwar still.
    *scratch = *game;
    scratch->world.number_of_entities = 0;

    RAD_JsonReader_t reader;
    RAD_JsonReaderInit(&reader, json, tokens, parsed);

    RAD_SerializeResult_t result = RAD_DeserializeRoot(&reader, scratch);
    if(result == RAD_SERIALIZE_OK)
    {
        // Die Tile-Ereignisse erst nach der Uebernahme und gegen den alten Stand:
        // ein misslungener Ladevorgang meldet nichts, ein gelungener genau die
        // Felder, die sich geaendert haben -- mit Zeigern in das Spiel und nicht
        // in den Zwischenstand, der gleich freigegeben wird.
        RAD_Tile_t previous[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH];
        memcpy(previous, game->world.tiles, sizeof(previous));
        const int32_t previous_width = game->world.width;
        const int32_t previous_height = game->world.height;

        *game = *scratch;

        RAD_WorldPublishTileChanges(&game->world, previous, previous_width, previous_height);
    }

    free(scratch);
    free(tokens);
    return result;
}

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
        case RAD_SERIALIZE_ERROR_BUFFER_TOO_SMALL:    return "Puffer zu klein";
        case RAD_SERIALIZE_ERROR_SYNTAX:              return "kein gueltiges JSON";
        case RAD_SERIALIZE_ERROR_SCHEMA:              return "unerwartete Struktur";
        case RAD_SERIALIZE_ERROR_FORMAT:              return "fremdes Format";
        case RAD_SERIALIZE_ERROR_VERSION:             return "nicht unterstuetzte Version";
        case RAD_SERIALIZE_ERROR_SIZE_MISMATCH:       return "Weltgroesse passt nicht";
        case RAD_SERIALIZE_ERROR_TILE_TYPE:           return "unbekannter Tile-Typ";
        case RAD_SERIALIZE_ERROR_ENTITY_TYPE:         return "unbekannter Entitaets-Typ";
        case RAD_SERIALIZE_ERROR_ENTITY_ID:           return "ungueltige oder doppelte Entitaets-Id";
        case RAD_SERIALIZE_ERROR_ENTITY_POSITION:     return "Entitaet ausserhalb der Welt";
        case RAD_SERIALIZE_ERROR_TILE_OCCUPIED:       return "zwei Entitaeten auf einem Tile";
        case RAD_SERIALIZE_ERROR_INCONSISTENT:        return "Datei widerspricht sich selbst";
        case RAD_SERIALIZE_ERROR_WORLD_OCCUPIED:      return "auf der Welt stehen schon Figuren";
        case RAD_SERIALIZE_ERROR_OUT_OF_MEMORY:       return "kein Speicher";
        default:                                      return "unbekannter Fehler";
    }
}

static RAD_SerializeResult_t RAD_DeserializeRoot(RAD_JsonReader_t *reader, RAD_Game_t *game)
{
    int32_t number_of_fields = 0;
    if(!RAD_JsonReadBeginObject(reader, &number_of_fields))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    bool have_format = false;
    bool have_version = false;

    // Der Rumpf wird zunaechst nur vermerkt und uebersprungen: erst wenn Format
    // und Version stimmen, wird er gelesen. Sonst meldet eine fremde Datei einen
    // Strukturfehler tief in der Welt, statt schlicht "fremdes Format".
    int32_t game_token = -1;

    for(int32_t i=0;i < number_of_fields; ++i)
    {
        char key[RAD_JSON_KEY_MAX];
        if(!RAD_JsonReadKey(reader, key, sizeof(key)))
        {
            return RAD_SERIALIZE_ERROR_SCHEMA;
        }

        if(strcmp(key, "format") == 0)
        {
            char name[RAD_JSON_NAME_MAX];
            if(!RAD_JsonReadString(reader, name, sizeof(name)))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
            if(strcmp(name, RAD_SAVE_FORMAT_NAME) != 0)
            {
                return RAD_SERIALIZE_ERROR_FORMAT;
            }
            have_format = true;
        }
        else if(strcmp(key, "version") == 0)
        {
            int32_t version = 0;
            if(!RAD_JsonReadInt(reader, &version))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
            if(version < RAD_SAVE_FORMAT_VERSION_MIN || version > RAD_SAVE_FORMAT_VERSION)
            {
                return RAD_SERIALIZE_ERROR_VERSION;
            }
            have_version = true;
        }
        else if(strcmp(key, "game") == 0)
        {
            game_token = reader->cursor;
            RAD_JsonSkipValue(reader);
        }
        else
        {
            RAD_JsonSkipValue(reader);
        }
    }

    if(!RAD_JsonReaderOk(reader))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }
    if(!have_format)
    {
        return RAD_SERIALIZE_ERROR_FORMAT;
    }
    if(!have_version)
    {
        return RAD_SERIALIZE_ERROR_VERSION;
    }
    if(game_token < 0)
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    reader->cursor = game_token;
    return RAD_DeserializeGame(reader, game);
}
