#include <radish/serialization/entity_serializer.h>
#include <string.h>

///
/// Nach RAD_EntityType_t indiziert -- die Reihenfolge muss zum enum passen.
///
static const char *entity_type_names[] = {
    "none",
    "player",
    "npc"
};

#define RAD_ENTITY_TYPE_COUNT ((int32_t)(sizeof(entity_type_names) / sizeof(entity_type_names[0])))

void RAD_SerializeEntity(RAD_JsonWriter_t *writer, const RAD_Entity_t *entity)
{
    RAD_JsonWriteBeginObject(writer);

    RAD_JsonWriteKey(writer, "id");
    RAD_JsonWriteInt(writer, entity->id);

    RAD_JsonWriteKey(writer, "type");
    RAD_JsonWriteString(writer, RAD_EntityTypeToString(entity->type));

    // Wie bei tile.entity: "nichts" ist null und nicht der Zahlwert, mit dem es
    // im Speicher hingeschrieben wird.
    RAD_JsonWriteKey(writer, "owner");
    if(entity->owner == RAD_USER_NONE)
    {
        RAD_JsonWriteNull(writer);
    }
    else
    {
        RAD_JsonWriteUInt64(writer, entity->owner);
    }

    RAD_JsonWriteKey(writer, "x");
    RAD_JsonWriteInt(writer, entity->x);

    RAD_JsonWriteKey(writer, "y");
    RAD_JsonWriteInt(writer, entity->y);

    RAD_JsonWriteEndObject(writer);
}

RAD_SerializeResult_t RAD_DeserializeEntity(RAD_JsonReader_t *reader, RAD_Entity_t *entity)
{
    int32_t number_of_fields = 0;
    if(!RAD_JsonReadBeginObject(reader, &number_of_fields))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    *entity = (RAD_Entity_t){
        .id = RAD_ENTITY_NONE,
        .type = RAD_ENTITY_TYPE_NONE,
        .owner = RAD_USER_NONE,
        .x = 0,
        .y = 0
    };

    for(int32_t i=0;i < number_of_fields; ++i)
    {
        char key[RAD_JSON_KEY_MAX];
        if(!RAD_JsonReadKey(reader, key, sizeof(key)))
        {
            return RAD_SERIALIZE_ERROR_SCHEMA;
        }

        if(strcmp(key, "id") == 0)
        {
            if(!RAD_JsonReadInt(reader, &entity->id))
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
            entity->type = RAD_EntityTypeFromString(name, &ok);
            if(!ok)
            {
                return RAD_SERIALIZE_ERROR_ENTITY_TYPE;
            }
        }
        else if(strcmp(key, "owner") == 0)
        {
            // Fehlt das Feld ganz, bleibt es beim Vorbesetzten: ein Stand aus der
            // Zeit vor dem Besitz laedt weiter, seine Figuren sind herrenlos.
            if(RAD_JsonPeekIsNull(reader))
            {
                RAD_JsonSkipValue(reader);
            }
            else if(!RAD_JsonReadUInt64(reader, &entity->owner))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
        }
        else if(strcmp(key, "x") == 0)
        {
            if(!RAD_JsonReadInt(reader, &entity->x))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
        }
        else if(strcmp(key, "y") == 0)
        {
            if(!RAD_JsonReadInt(reader, &entity->y))
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

const char* RAD_EntityTypeToString(RAD_EntityType_t type)
{
    if((int32_t)type < 0 || (int32_t)type >= RAD_ENTITY_TYPE_COUNT)
    {
        return entity_type_names[RAD_ENTITY_TYPE_NONE];
    }
    return entity_type_names[type];
}

RAD_EntityType_t RAD_EntityTypeFromString(const char *name, bool *ok)
{
    for(int32_t i=0;i < RAD_ENTITY_TYPE_COUNT; ++i)
    {
        if(strcmp(name, entity_type_names[i]) == 0)
        {
            if(ok != NULL)
            {
                *ok = true;
            }
            return (RAD_EntityType_t)i;
        }
    }

    if(ok != NULL)
    {
        *ok = false;
    }
    return RAD_ENTITY_TYPE_NONE;
}
