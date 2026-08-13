#include <radish/serialization/tile_serializer.h>
#include <string.h>

///
/// Nach RAD_TileType_t indiziert -- die Reihenfolge muss zum enum passen.
///
static const char *tile_type_names[] = {
    "void",
    "ground",
    "water"
};

#define RAD_TILE_TYPE_COUNT ((int32_t)(sizeof(tile_type_names) / sizeof(tile_type_names[0])))

void RAD_SerializeTile(RAD_JsonWriter_t *writer, const RAD_Tile_t *tile)
{
    RAD_JsonWriteBeginObject(writer);

    RAD_JsonWriteKey(writer, "x");
    RAD_JsonWriteInt(writer, tile->x);

    RAD_JsonWriteKey(writer, "y");
    RAD_JsonWriteInt(writer, tile->y);

    RAD_JsonWriteKey(writer, "type");
    RAD_JsonWriteString(writer, RAD_TileTypeToString(tile->type));

    // Die magische -1 bleibt in der Implementierung; JSON hat fuer "nichts" null.
    RAD_JsonWriteKey(writer, "entity");
    if(tile->entity == RAD_ENTITY_NONE)
    {
        RAD_JsonWriteNull(writer);
    }
    else
    {
        RAD_JsonWriteInt(writer, tile->entity);
    }

    RAD_JsonWriteEndObject(writer);
}

RAD_SerializeResult_t RAD_DeserializeTile(RAD_JsonReader_t *reader, RAD_Tile_t *tile)
{
    int32_t number_of_fields = 0;
    if(!RAD_JsonReadBeginObject(reader, &number_of_fields))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    *tile = (RAD_Tile_t){
        .x = 0,
        .y = 0,
        .type = RAD_TILE_TYPE_VOID,
        .entity = RAD_ENTITY_NONE
    };

    for(int32_t i=0;i < number_of_fields; ++i)
    {
        char key[RAD_JSON_KEY_MAX];
        if(!RAD_JsonReadKey(reader, key, sizeof(key)))
        {
            return RAD_SERIALIZE_ERROR_SCHEMA;
        }

        if(strcmp(key, "x") == 0)
        {
            if(!RAD_JsonReadInt(reader, &tile->x))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
        }
        else if(strcmp(key, "y") == 0)
        {
            if(!RAD_JsonReadInt(reader, &tile->y))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
        }
        else if(strcmp(key, "type") == 0)
        {
            char name[RAD_JSON_NAME_MAX];
            if(!RAD_JsonReadString(reader, name, sizeof(name)))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }

            bool ok = false;
            tile->type = RAD_TileTypeFromString(name, &ok);
            if(!ok)
            {
                return RAD_SERIALIZE_ERROR_TILE_TYPE;
            }
        }
        else if(strcmp(key, "entity") == 0)
        {
            if(RAD_JsonPeekIsNull(reader))
            {
                RAD_JsonSkipValue(reader);
                tile->entity = RAD_ENTITY_NONE;
            }
            else if(!RAD_JsonReadInt(reader, &tile->entity))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
        }
        else
        {
            // Unbekanntes Feld: ueberspringen statt scheitern.
            RAD_JsonSkipValue(reader);
        }
    }

    return RAD_JsonReaderOk(reader) ? RAD_SERIALIZE_OK : RAD_SERIALIZE_ERROR_SCHEMA;
}

const char* RAD_TileTypeToString(RAD_TileType_t type)
{
    if((int32_t)type < 0 || (int32_t)type >= RAD_TILE_TYPE_COUNT)
    {
        return tile_type_names[RAD_TILE_TYPE_VOID];
    }
    return tile_type_names[type];
}

RAD_TileType_t RAD_TileTypeFromString(const char *name, bool *ok)
{
    for(int32_t i=0;i < RAD_TILE_TYPE_COUNT; ++i)
    {
        if(strcmp(name, tile_type_names[i]) == 0)
        {
            if(ok != NULL)
            {
                *ok = true;
            }
            return (RAD_TileType_t)i;
        }
    }

    if(ok != NULL)
    {
        *ok = false;
    }
    return RAD_TILE_TYPE_VOID;
}
