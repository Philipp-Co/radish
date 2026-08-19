#include <radish/game/control/command/move_entity.h>


void RAD_SerializeCommandMoveEntity(RAD_ByteWriter_t *writer, const RAD_CommandMoveEntity_t *command)
{
    RAD_ByteWriteInt32(writer, command->entity);
    RAD_ByteWriteInt8(writer, command->path.number_of_steps);

    // Alle Plaetze und nicht nur die belegten: die Laenge der Nutzlast haengt
    // nicht an der Anzahl der Schritte (move_entity.h). Was hinter dem Zaehler
    // liegt, geht als (0,0) heraus statt so, wie es im Speicher steht -- dieselbe
    // Bewegung soll immer dieselbe Bytefolge ergeben. Ein Zaehler ausserhalb
    // seiner Grenzen nullt damit alles; abgewiesen wird er beim Lesen.
    for(int32_t i = 0; i < RAD_PATH_MAX_STEPS; ++i)
    {
        const bool used = (i < command->path.number_of_steps);

        RAD_ByteWriteInt16(writer, used ? command->path.steps_to[i].x : 0);
        RAD_ByteWriteInt16(writer, used ? command->path.steps_to[i].y : 0);
    }
}

RAD_CommandCodecResult_t RAD_DeserializeCommandMoveEntity(RAD_ByteReader_t *reader, RAD_CommandMoveEntity_t *command)
{
    RAD_EntityId_t entity = 0;
    int8_t number_of_steps = 0;
    RAD_EntityPosition_t steps_to[RAD_PATH_MAX_STEPS] = {0};

    RAD_ByteReadInt32(reader, &entity);
    RAD_ByteReadInt8(reader, &number_of_steps);

    for(int32_t i = 0; i < RAD_PATH_MAX_STEPS; ++i)
    {
        RAD_ByteReadInt16(reader, &steps_to[i].x);
        RAD_ByteReadInt16(reader, &steps_to[i].y);
    }

    // Einmal am Ende gefragt statt bei jedem der vierunddreissig Felder: der
    // Fehler des Readers klebt (byte_reader.h). Nach dem ersten Fehlschlag
    // liefert jedes weitere Lesen false und laesst den Cursor stehen, die
    // Reihenfolge der Pruefungen ist damit gleichgueltig.
    if(!RAD_ByteReaderOk(reader))
    {
        return RAD_COMMAND_CODEC_ERROR_TRUNCATED;
    }

    if((number_of_steps < 1) || (number_of_steps > RAD_PATH_MAX_STEPS))
    {
        return RAD_COMMAND_CODEC_ERROR_INVALID_STEP_COUNT;
    }

    command->entity = entity;
    command->path.number_of_steps = number_of_steps;

    for(int32_t i = 0; i < RAD_PATH_MAX_STEPS; ++i)
    {
        // Hinter dem Zaehler wird genullt und nicht uebernommen. Auf der Strecke
        // stand dort schon (0,0); dass es hier noch einmal steht, macht das
        // Ergebnis unabhaengig davon, was die Gegenseite geschrieben hat.
        const bool used = (i < number_of_steps);

        command->path.steps_to[i].x = used ? steps_to[i].x : 0;
        command->path.steps_to[i].y = used ? steps_to[i].y : 0;
    }

    return RAD_COMMAND_CODEC_OK;
}
