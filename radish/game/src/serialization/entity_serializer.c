#include <radish/game/serialization/entity_serializer.h>
#include <stdint.h>
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

static RAD_SerializeResult_t RAD_DeserializeCoordinate(RAD_JsonReader_t *reader, int16_t *out);

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
            RAD_SerializeResult_t result = RAD_DeserializeCoordinate(reader, &entity->x);
            if(result != RAD_SERIALIZE_OK)
            {
                return result;
            }
        }
        else if(strcmp(key, "y") == 0)
        {
            RAD_SerializeResult_t result = RAD_DeserializeCoordinate(reader, &entity->y);
            if(result != RAD_SERIALIZE_OK)
            {
                return result;
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

///
/// Liest eine Koordinate in ein int16_t-Feld.
///
/// **Der Umweg ueber die breite Variable ist der Punkt.** RAD_JsonReadInt
/// schreibt vier Byte durch den Zeiger, den es bekommt; entity->x und entity->y
/// sind zwei Byte breit (model/entity/entity.h). Direkt uebergeben -- so stand es
/// hier -- schrieb der Leser also zwei Byte ueber das Feld hinaus, und zwar in das
/// naechste: bei "x" in y, bei "y" in health. Ein Stand mit gueltigen Koordinaten
/// kam dabei trotzdem richtig heraus, solange die Bytes danach wieder
/// ueberschrieben wurden -- weshalb es nur eine Warnung war und nichts aufgefallen
/// ist.
///
/// **Danach die Bereichsgrenze**, denn geprueft ist der Wert damit noch nicht: in
/// der Datei steht eine beliebige Zahl, und alles jenseits von int16_t liesse sich
/// nicht hinschreiben, ohne dass es etwas anderes wird. Was aus so einem Wert
/// wird, entscheidet damit hier und nicht der Compiler.
///
/// Die Welt kommt dabei nicht vor: ob (x,y) auf dem Raster liegt, prueft der
/// World-Serializer, der die Welt hat -- mit demselben Fehlercode. Hier geht es
/// nur um das Feld, in das der Wert soll.
///
static RAD_SerializeResult_t RAD_DeserializeCoordinate(RAD_JsonReader_t *reader, int16_t *out)
{
    int32_t value = 0;
    if(!RAD_JsonReadInt(reader, &value))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    if(value < INT16_MIN || value > INT16_MAX)
    {
        return RAD_SERIALIZE_ERROR_ENTITY_POSITION;
    }

    *out = (int16_t)value;
    return RAD_SERIALIZE_OK;
}
