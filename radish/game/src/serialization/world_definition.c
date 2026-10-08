#include <radish/game/serialization/world_definition.h>
#include <radish/game/model/game.h>
#include <stdlib.h>
#include <string.h>

///
/// Ein Raster aus der Datei, noch als Text: "rows" oder "heights". Eine Zeile
/// mehr Platz als die Welt breit sein darf, fuer die abschliessende Null.
///
typedef struct
{
    char cells[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH + 1];
    int32_t width;
    int32_t height;
} RAD_WorldDefinitionGrid_t;

static RAD_SerializeResult_t RAD_ReadGrid(RAD_JsonReader_t *reader, RAD_WorldDefinitionGrid_t *grid);
static bool RAD_TileTypeFromTerrain(char terrain, RAD_TileType_t *type);


RAD_SerializeResult_t RAD_DeserializeWorldDefinition(RAD_JsonReader_t *reader, RAD_World_t *world)
{
    int32_t number_of_fields = 0;
    if(!RAD_JsonReadBeginObject(reader, &number_of_fields))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    // Die Raster werden zunaechst nur vermerkt und uebersprungen: erst wenn die
    // Version stimmt, wird gelesen. Sonst meldet eine Datei aus einer spaeteren
    // Version einen Fehler in ihren Zeilen statt schlicht "andere Version".
    int32_t version = -1;
    int32_t rows_token = -1;
    int32_t heights_token = -1;

    for(int32_t i=0;i < number_of_fields; ++i)
    {
        char key[RAD_JSON_KEY_MAX];
        if(!RAD_JsonReadKey(reader, key, sizeof(key)))
        {
            return RAD_SERIALIZE_ERROR_SCHEMA;
        }

        if(strcmp(key, "version") == 0)
        {
            if(!RAD_JsonReadInt(reader, &version))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
        }
        else if(strcmp(key, "rows") == 0)
        {
            rows_token = reader->cursor;
            RAD_JsonSkipValue(reader);
        }
        else if(strcmp(key, "heights") == 0)
        {
            heights_token = reader->cursor;
            RAD_JsonSkipValue(reader);
        }
        else if(strcmp(key, "name") == 0)
        {
            char name[RAD_JSON_NAME_MAX];
            if(!RAD_JsonReadString(reader, name, sizeof(name)))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
        }
        else if(strcmp(key, "$schema") == 0)
        {
            int32_t length = 0;
            if(!RAD_JsonPeekStringLength(reader, &length))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
            RAD_JsonSkipValue(reader);
        }
        else
        {
            // Unbekannte Schluessel sind ein Fehler (world_definition.h).
            return RAD_SERIALIZE_ERROR_SCHEMA;
        }
    }

    if(!RAD_JsonReaderOk(reader))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }
    if(version < 0)
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }
    if(version != RAD_WORLD_DEFINITION_VERSION)
    {
        return RAD_SERIALIZE_ERROR_VERSION;
    }
    if(rows_token < 0)
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    // Zurueck zu den Rastern, und danach wieder ans Ende des Objekts -- dahinter
    // liest niemand mehr, aber der Cursor soll stehen, wo ein Leser ihn erwartet.
    const int32_t end_token = reader->cursor;

    // Auf dem Heap: zwei Raster sind zusammen nicht gross, aber sie wachsen mit
    // der Obergrenze der Welt, und die soll steigen duerfen, ohne dass dieser
    // Aufrufrahmen mitwaechst.
    RAD_WorldDefinitionGrid_t *rows = malloc(sizeof(RAD_WorldDefinitionGrid_t));
    RAD_WorldDefinitionGrid_t *heights = malloc(sizeof(RAD_WorldDefinitionGrid_t));
    if(rows == NULL || heights == NULL)
    {
        free(rows);
        free(heights);
        return RAD_SERIALIZE_ERROR_OUT_OF_MEMORY;
    }

    reader->cursor = rows_token;
    RAD_SerializeResult_t result = RAD_ReadGrid(reader, rows);

    if(result == RAD_SERIALIZE_OK && heights_token >= 0)
    {
        reader->cursor = heights_token;
        result = RAD_ReadGrid(reader, heights);

        if(result == RAD_SERIALIZE_OK && (heights->width != rows->width || heights->height != rows->height))
        {
            result = RAD_SERIALIZE_ERROR_SIZE_MISMATCH;
        }
    }

    reader->cursor = end_token;

    // Erst alles pruefen, dann schreiben: die Welt bleibt bei jedem Fehler, wie sie
    // war. Die Typen und Hoehen stehen dafuer zunaechst hier.
    RAD_TileType_t types[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH];
    int32_t z[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH];

    for(int32_t y=0;(result == RAD_SERIALIZE_OK) && (y < rows->height); ++y)
    {
        for(int32_t x=0;x < rows->width; ++x)
        {
            if(!RAD_TileTypeFromTerrain(rows->cells[y][x], &types[y][x]))
            {
                result = RAD_SERIALIZE_ERROR_TILE_TYPE;
                break;
            }

            z[y][x] = 0;
            if(heights_token < 0)
            {
                continue;
            }

            const char height = heights->cells[y][x];
            if(height < '0' || height > '9')
            {
                result = RAD_SERIALIZE_ERROR_SCHEMA;
                break;
            }
            z[y][x] = height - '0';

            if(types[y][x] == RAD_TILE_TYPE_VOID && z[y][x] != 0)
            {
                result = RAD_SERIALIZE_ERROR_INCONSISTENT;
                break;
            }
        }
    }

    if(result == RAD_SERIALIZE_OK)
    {
        // Die Groesse ist in RAD_ReadGrid schon gegen die Obergrenze geprueft --
        // RAD_ResetWorldToSize kann hier nicht mehr ablehnen.
        RAD_ResetWorldToSize(world, rows->width, rows->height);
        for(int32_t y=0;y < rows->height; ++y)
        {
            for(int32_t x=0;x < rows->width; ++x)
            {
                world->tiles[y][x].type = types[y][x];
                world->tiles[y][x].z = z[y][x];
            }
        }
    }

    free(rows);
    free(heights);
    return result;
}

RAD_SerializeResult_t RAD_DeserializeWorldDefinitionFromJson(RAD_Game_t *game, const char *json, size_t length)
{
    if(game == NULL || json == NULL)
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    if(RAD_UnitPoolNumberOfUnits(game->unit_pool) != 0)
    {
        return RAD_SERIALIZE_ERROR_WORLD_OCCUPIED;
    }

    jsmntok_t *tokens = NULL;
    int32_t number_of_tokens = 0;
    RAD_SerializeResult_t result = RAD_JsonTokenize(json, length, &tokens, &number_of_tokens);
    if(result != RAD_SERIALIZE_OK)
    {
        return result;
    }

    // Ohne Zwischenstand: RAD_DeserializeWorldDefinition schreibt erst, wenn alles
    // geprueft ist, und aus dem Spiel ausser der Welt kommt nichts vor.
    // Festgehalten wird nur, was die Meldung danach braucht.
    RAD_Tile_t previous[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH];
    memcpy(previous, game->world.tiles, sizeof(previous));
    const int32_t previous_width = game->world.width;
    const int32_t previous_height = game->world.height;

    RAD_JsonReader_t reader;
    RAD_JsonReaderInit(&reader, json, tokens, number_of_tokens);

    result = RAD_DeserializeWorldDefinition(&reader, &game->world);
    if(result == RAD_SERIALIZE_OK)
    {
        RAD_WorldPublishTileChanges(&game->world, previous, previous_width, previous_height);
    }

    free(tokens);
    return result;
}

///
/// Ein Array aus Strings gleicher Laenge, als Raster. Die Grenzen der Welt werden
/// hier geprueft und nicht beim Aufbau: eine zu breite Zeile passt gar nicht erst
/// in "cells", und die Laenge muss deshalb vor dem Lesen bekannt sein.
///
static RAD_SerializeResult_t RAD_ReadGrid(RAD_JsonReader_t *reader, RAD_WorldDefinitionGrid_t *grid)
{
    int32_t number_of_rows = 0;
    if(!RAD_JsonReadBeginArray(reader, &number_of_rows))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }
    if(number_of_rows < 1 || number_of_rows > RAD_WORLD_HEIGHT)
    {
        return RAD_SERIALIZE_ERROR_SIZE_MISMATCH;
    }

    grid->height = number_of_rows;
    grid->width = -1;

    for(int32_t y=0;y < number_of_rows; ++y)
    {
        int32_t length = 0;
        if(!RAD_JsonPeekStringLength(reader, &length))
        {
            return RAD_SERIALIZE_ERROR_SCHEMA;
        }
        if(length < 1 || length > RAD_WORLD_WIDTH)
        {
            return RAD_SERIALIZE_ERROR_SIZE_MISMATCH;
        }

        // Die erste Zeile setzt die Breite, jede weitere muss ihr folgen.
        if(grid->width < 0)
        {
            grid->width = length;
        }
        else if(length != grid->width)
        {
            return RAD_SERIALIZE_ERROR_SIZE_MISMATCH;
        }

        if(!RAD_JsonReadString(reader, grid->cells[y], sizeof(grid->cells[y])))
        {
            return RAD_SERIALIZE_ERROR_SCHEMA;
        }
    }

    return RAD_SERIALIZE_OK;
}

///
/// Die Zeichen aus world.schema.json. Ein neuer Tile-Typ braucht hier ein neues
/// Zeichen und dort eins im Muster von "terrainRow".
///
static bool RAD_TileTypeFromTerrain(char terrain, RAD_TileType_t *type)
{
    switch(terrain)
    {
        case '_': *type = RAD_TILE_TYPE_VOID;   return true;
        case '.': *type = RAD_TILE_TYPE_GROUND; return true;
        case '~': *type = RAD_TILE_TYPE_WATER;  return true;
        default:                                return false;
    }
}
