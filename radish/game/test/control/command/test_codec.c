#include <unity.h>
#include <radish/game/control/command/codec.h>
#include <radish/game/control/command/byte_writer.h>
#include <radish/game/control/command/byte_reader.h>

#include <string.h>

///
/// Ein Move-Kommando durch Schreiber und Leser, damit die Probe an einem Puffer
/// haengt und nicht an einer Struktur.
///
/// Der Leser bekommt genau die geschriebene Laenge und nicht den ganzen Puffer:
/// eine Nachricht traegt genau ein Kommando, und was dahinter noch steht, ist
/// RAD_COMMAND_CODEC_ERROR_TRAILING_BYTES (codec.h).
///
static RAD_CommandCodecResult_t durch_den_codec(const RAD_Command_t *in, RAD_Command_t *out)
{
    uint8_t buffer[256];

    RAD_ByteWriter_t writer;
    RAD_ByteWriterInit(&writer, buffer, sizeof(buffer));
    RAD_SerializeCommand(&writer, in);
    TEST_ASSERT_TRUE(RAD_ByteWriterOk(&writer));

    RAD_ByteReader_t reader;
    RAD_ByteReaderInit(&reader, buffer, writer.length);
    return RAD_DeserializeCommand(&reader, out);
}

///
/// Baut ein Move-Kommando mit "felder" Feldern im Pfad. Die Felder selbst sind
/// eine Reihe nach rechts -- worauf sie zeigen, ist dem Codec gleich, er prueft
/// nur die Anzahl.
///
static RAD_Command_t move_mit_feldern(int8_t felder)
{
    RAD_Command_t command = {0};

    command.header.type = RAD_COMMAND_TYPE_MOVE_ENTITY;
    command.header.sequence = 1;
    command.header.user = (RAD_UserId_t)0x4711;

    command.command.move_entity.entity = 3;
    command.command.move_entity.path.number_of_steps = felder;

    for(int32_t i = 0; i < felder; ++i)
    {
        command.command.move_entity.path.steps_to[i].x = (int16_t)i;
        command.command.move_entity.path.steps_to[i].y = 2;
    }

    return command;
}

///
/// Die Wire-Nummern sind eine Zusage an die Gegenseite: sie haengen nicht an der
/// Reihenfolge im enum und duerfen sich nicht verschieben. Dieser Test haelt sie
/// fest -- der Rest der Codec-Tests kommt aus dem bisherigen Pruefprogramm.
///
void test_codec_kennt_die_wire_nummern(void)
{
    TEST_ASSERT_EQUAL_UINT8(2, RAD_CommandTypeToWire(RAD_COMMAND_TYPE_MOVE_ENTITY));
    TEST_ASSERT_EQUAL_UINT8(6, RAD_CommandTypeToWire(RAD_COMMAND_TYPE_END_TURN));

    // Die reservierte 0 gehoert zu keiner Art.
    bool ok = true;
    RAD_CommandTypeFromWire(0, &ok);
    TEST_ASSERT_FALSE(ok);

    RAD_CommandType_t type = RAD_CommandTypeFromWire(2, &ok);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_TYPE_MOVE_ENTITY, type);
}

///
/// **Der Pfad kommt heraus, wie er hineinging -- Startfeld zuerst.**
///
/// Seit steps_to[0] das Feld ist, auf dem die Figur schon steht (path.h), traegt
/// ein Kommando ein Feld mehr als es Schritte macht. Der Codec zaehlt Felder, und
/// dieser Test haelt fest, dass er sie unveraendert durchreicht: waere die Reihe um
/// einen Platz verschoben, kaeme die Figur woanders an.
///
void test_codec_reicht_den_pfad_mit_startfeld_durch(void)
{
    const RAD_Command_t in = move_mit_feldern(3);

    RAD_Command_t out = {0};
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_CODEC_OK, durch_den_codec(&in, &out));

    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_TYPE_MOVE_ENTITY, out.header.type);
    TEST_ASSERT_EQUAL_INT(3, out.command.move_entity.entity);

    TEST_ASSERT_EQUAL_INT(3, out.command.move_entity.path.number_of_steps);
    for(int32_t i = 0; i < 3; ++i)
    {
        TEST_ASSERT_EQUAL_INT(i, out.command.move_entity.path.steps_to[i].x);
        TEST_ASSERT_EQUAL_INT(2, out.command.move_entity.path.steps_to[i].y);
    }

    // Hinter dem Zaehler stehen Nullen und kein Rest -- die ungenutzten Plaetze
    // fahren mit (move_entity.h).
    for(int32_t i = 3; i < RAD_PATH_MAX_STEPS; ++i)
    {
        TEST_ASSERT_EQUAL_INT(0, out.command.move_entity.path.steps_to[i].x);
        TEST_ASSERT_EQUAL_INT(0, out.command.move_entity.path.steps_to[i].y);
    }
}

///
/// **Die Grenze der Feldanzahl, und sie liegt bei zwei.**
///
/// Zwei Felder sind ein Schritt und das Wenigste, was ein Weg sein kann. Eines
/// nennt nur, wo die Figur steht, null gar nichts -- beides ist keine Bewegung und
/// wird abgewiesen, statt als Kommando ohne Wirkung durchzugehen.
///
/// Die untere Grenze war einmal eins, als der Pfad ohne Startfeld auskam. Dieser
/// Test ist der Grund, aus dem eine Ruecknahme der Umstellung nicht still bliebe.
///
void test_codec_weist_pfade_ohne_weg_ab(void)
{
    RAD_Command_t out = {0};

    const RAD_Command_t ohne_felder = move_mit_feldern(0);
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_CODEC_ERROR_INVALID_STEP_COUNT,
                          durch_den_codec(&ohne_felder, &out));

    const RAD_Command_t nur_standort = move_mit_feldern(1);
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_CODEC_ERROR_INVALID_STEP_COUNT,
                          durch_den_codec(&nur_standort, &out));

    // Zwei geht.
    const RAD_Command_t ein_schritt = move_mit_feldern(2);
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_CODEC_OK, durch_den_codec(&ein_schritt, &out));
    TEST_ASSERT_EQUAL_INT(2, out.command.move_entity.path.number_of_steps);

    // Die obere Grenze auch, und einer darueber nicht mehr. RAD_PATH_MAX_STEPS
    // zaehlt Felder, also sind es hoechstens RAD_PATH_MAX_STEPS-1 Schritte.
    const RAD_Command_t voll = move_mit_feldern((int8_t)RAD_PATH_MAX_STEPS);
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_CODEC_OK, durch_den_codec(&voll, &out));

    RAD_Command_t zu_lang = move_mit_feldern((int8_t)RAD_PATH_MAX_STEPS);
    zu_lang.command.move_entity.path.number_of_steps = (int8_t)(RAD_PATH_MAX_STEPS + 1);
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_CODEC_ERROR_INVALID_STEP_COUNT,
                          durch_den_codec(&zu_lang, &out));

    // Eine negative Anzahl faellt auf wie eine zu grosse -- der Zaehler ist int8_t
    // und wird als Zweierkomplement gelesen (path.h).
    RAD_Command_t negativ = move_mit_feldern(2);
    negativ.command.move_entity.path.number_of_steps = -1;
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_CODEC_ERROR_INVALID_STEP_COUNT,
                          durch_den_codec(&negativ, &out));
}
