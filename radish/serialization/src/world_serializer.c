#include <radish/serialization/world_serializer.h>
#include <radish/serialization/tile_serializer.h>
#include <radish/serialization/entity_serializer.h>
#include <string.h>

static RAD_SerializeResult_t RAD_DeserializeTileRows(
    RAD_JsonReader_t *reader,
    RAD_TileType_t types[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH],
    RAD_EntityId_t tile_entity[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH]
);
static RAD_SerializeResult_t RAD_DeserializeEntityList(
    RAD_JsonReader_t *reader,
    RAD_Entity_t *entities,
    int32_t *number_of_entities
);

void RAD_SerializeWorld(RAD_JsonWriter_t *writer, const RAD_World_t *world)
{
    RAD_JsonWriteBeginObject(writer);

    RAD_JsonWriteKey(writer, "width");
    RAD_JsonWriteInt(writer, RAD_WORLD_WIDTH);

    RAD_JsonWriteKey(writer, "height");
    RAD_JsonWriteInt(writer, RAD_WORLD_HEIGHT);

    RAD_JsonWriteKey(writer, "tiles");
    RAD_JsonWriteBeginArray(writer);
    for(int32_t y=0;y < RAD_WORLD_HEIGHT; ++y)
    {
        RAD_JsonWriteBeginArray(writer);
        for(int32_t x=0;x < RAD_WORLD_WIDTH; ++x)
        {
            RAD_SerializeTile(writer, &world->tiles[y][x]);
        }
        RAD_JsonWriteEndArray(writer);
    }
    RAD_JsonWriteEndArray(writer);

    RAD_JsonWriteKey(writer, "entities");
    RAD_JsonWriteBeginArray(writer);
    for(RAD_EntityId_t i=0;i < RAD_MAX_ENTITIES; ++i)
    {
        if(world->entities[i].id != RAD_ENTITY_NONE)
        {
            RAD_SerializeEntity(writer, &world->entities[i]);
        }
    }
    RAD_JsonWriteEndArray(writer);

    RAD_JsonWriteEndObject(writer);
}

RAD_SerializeResult_t RAD_DeserializeWorld(RAD_JsonReader_t *reader, RAD_World_t *world)
{
    int32_t number_of_fields = 0;
    if(!RAD_JsonReadBeginObject(reader, &number_of_fields))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }

    // Erst vollstaendig einlesen, dann aufbauen -- so ist die Reihenfolge der
    // Felder im JSON egal und die Welt wird nur aus geprueften Werten gebaut.
    int32_t width = -1;
    int32_t height = -1;

    RAD_TileType_t types[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH];
    RAD_EntityId_t tile_entity[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH];
    RAD_Entity_t entities[RAD_MAX_ENTITIES];
    int32_t number_of_entities = 0;

    bool have_tiles = false;
    bool have_entities = false;

    for(int32_t y=0;y < RAD_WORLD_HEIGHT; ++y)
    {
        for(int32_t x=0;x < RAD_WORLD_WIDTH; ++x)
        {
            types[y][x] = RAD_TILE_TYPE_VOID;
            tile_entity[y][x] = RAD_ENTITY_NONE;
        }
    }

    for(int32_t i=0;i < number_of_fields; ++i)
    {
        char key[RAD_JSON_KEY_MAX];
        if(!RAD_JsonReadKey(reader, key, sizeof(key)))
        {
            return RAD_SERIALIZE_ERROR_SCHEMA;
        }

        if(strcmp(key, "width") == 0)
        {
            if(!RAD_JsonReadInt(reader, &width))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
        }
        else if(strcmp(key, "height") == 0)
        {
            if(!RAD_JsonReadInt(reader, &height))
            {
                return RAD_SERIALIZE_ERROR_SCHEMA;
            }
        }
        else if(strcmp(key, "tiles") == 0)
        {
            RAD_SerializeResult_t result = RAD_DeserializeTileRows(reader, types, tile_entity);
            if(result != RAD_SERIALIZE_OK)
            {
                return result;
            }
            have_tiles = true;
        }
        else if(strcmp(key, "entities") == 0)
        {
            RAD_SerializeResult_t result = RAD_DeserializeEntityList(reader, entities, &number_of_entities);
            if(result != RAD_SERIALIZE_OK)
            {
                return result;
            }
            have_entities = true;
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
    if(!have_tiles || !have_entities || width < 0 || height < 0)
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }
    if(width != RAD_WORLD_WIDTH || height != RAD_WORLD_HEIGHT)
    {
        return RAD_SERIALIZE_ERROR_SIZE_MISMATCH;
    }

    RAD_InitWorld(world);
    for(int32_t y=0;y < RAD_WORLD_HEIGHT; ++y)
    {
        for(int32_t x=0;x < RAD_WORLD_WIDTH; ++x)
        {
            world->tiles[y][x].type = types[y][x];
        }
    }

    for(int32_t i=0;i < number_of_entities; ++i)
    {
        const RAD_Entity_t *entity = &entities[i];

        // Vorab getrennt pruefen, damit der Fehlercode sagt, was los ist --
        // RAD_WorldSpawnEntityWithId selbst kennt nur "ging nicht".
        if(entity->id < 0 || entity->id >= RAD_MAX_ENTITIES || RAD_WorldEntityById(world, entity->id) != NULL)
        {
            return RAD_SERIALIZE_ERROR_ENTITY_ID;
        }
        if(!RAD_WorldInBounds(world, entity->x, entity->y))
        {
            return RAD_SERIALIZE_ERROR_ENTITY_POSITION;
        }
        if(RAD_WorldTileAt(world, entity->x, entity->y)->entity != RAD_ENTITY_NONE)
        {
            return RAD_SERIALIZE_ERROR_TILE_OCCUPIED;
        }

        if(RAD_WorldSpawnEntityWithId(world, entity->id, entity->type, entity->x, entity->y) == RAD_ENTITY_NONE)
        {
            return RAD_SERIALIZE_ERROR_ENTITY_ID;
        }
    }

    // Gegenprobe: die Zuordnung aus der Datei muss der aus der Entitaetsliste
    // wieder aufgebauten entsprechen. Damit ist tile.entity keine Zierde,
    // sondern eine gepruefte Angabe.
    for(int32_t y=0;y < RAD_WORLD_HEIGHT; ++y)
    {
        for(int32_t x=0;x < RAD_WORLD_WIDTH; ++x)
        {
            if(tile_entity[y][x] != world->tiles[y][x].entity)
            {
                return RAD_SERIALIZE_ERROR_INCONSISTENT;
            }
        }
    }

    if(!RAD_WorldIsConsistent(world))
    {
        return RAD_SERIALIZE_ERROR_INCONSISTENT;
    }

    return RAD_SERIALIZE_OK;
}

static RAD_SerializeResult_t RAD_DeserializeTileRows(
    RAD_JsonReader_t *reader,
    RAD_TileType_t types[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH],
    RAD_EntityId_t tile_entity[RAD_WORLD_HEIGHT][RAD_WORLD_WIDTH])
{
    int32_t number_of_rows = 0;
    if(!RAD_JsonReadBeginArray(reader, &number_of_rows))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }
    if(number_of_rows != RAD_WORLD_HEIGHT)
    {
        return RAD_SERIALIZE_ERROR_SIZE_MISMATCH;
    }

    for(int32_t y=0;y < number_of_rows; ++y)
    {
        int32_t number_of_columns = 0;
        if(!RAD_JsonReadBeginArray(reader, &number_of_columns))
        {
            return RAD_SERIALIZE_ERROR_SCHEMA;
        }
        if(number_of_columns != RAD_WORLD_WIDTH)
        {
            return RAD_SERIALIZE_ERROR_SIZE_MISMATCH;
        }

        for(int32_t x=0;x < number_of_columns; ++x)
        {
            RAD_Tile_t tile;
            RAD_SerializeResult_t result = RAD_DeserializeTile(reader, &tile);
            if(result != RAD_SERIALIZE_OK)
            {
                return result;
            }

            // Das Tile traegt seine Position selbst; sie muss zu der Stelle
            // passen, an der es im Raster steht.
            if(tile.x != x || tile.y != y)
            {
                return RAD_SERIALIZE_ERROR_INCONSISTENT;
            }

            types[y][x] = tile.type;
            tile_entity[y][x] = tile.entity;
        }
    }

    return RAD_SERIALIZE_OK;
}

static RAD_SerializeResult_t RAD_DeserializeEntityList(
    RAD_JsonReader_t *reader,
    RAD_Entity_t *entities,
    int32_t *number_of_entities)
{
    int32_t count = 0;
    if(!RAD_JsonReadBeginArray(reader, &count))
    {
        return RAD_SERIALIZE_ERROR_SCHEMA;
    }
    if(count > RAD_MAX_ENTITIES)
    {
        return RAD_SERIALIZE_ERROR_ENTITY_ID;
    }

    for(int32_t i=0;i < count; ++i)
    {
        RAD_SerializeResult_t result = RAD_DeserializeEntity(reader, &entities[i]);
        if(result != RAD_SERIALIZE_OK)
        {
            return result;
        }
    }

    *number_of_entities = count;
    return RAD_SERIALIZE_OK;
}
