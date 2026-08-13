#include <radish/serialization/game_serializer.h>
#include <radish/serialization/world_serializer.h>
#include <string.h>

void RAD_SerializeGame(RAD_JsonWriter_t *writer, const RAD_Game_t *game)
{
    RAD_JsonWriteBeginObject(writer);

    RAD_JsonWriteKey(writer, "world");
    RAD_SerializeWorld(writer, &game->world);

    RAD_JsonWriteEndObject(writer);
}

RAD_SerializeResult_t RAD_DeserializeGame(RAD_JsonReader_t *reader, RAD_Game_t *game)
{
    int32_t number_of_fields = 0;
    if(!RAD_JsonReadBeginObject(reader, &number_of_fields))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    bool have_world = false;

    for(int32_t i=0;i < number_of_fields; ++i)
    {
        char key[RAD_JSON_KEY_MAX];
        if(!RAD_JsonReadKey(reader, key, sizeof(key)))
        {
            return RAD_SERIALIZE_ERROR_SCHEMA;
        }

        if(strcmp(key, "world") == 0)
        {
            RAD_SerializeResult_t result = RAD_DeserializeWorld(reader, &game->world);
            if(result != RAD_SERIALIZE_OK)
            {
                return result;
            }
            have_world = true;
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
    if(!have_world)
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    return RAD_SERIALIZE_OK;
}
