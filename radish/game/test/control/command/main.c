#include <unity.h>

///
/// Der Runner fuer control/command. Unity ruft die Tests nicht selbst auf -- ohne den
/// Ruby-Generator zaehlt sie diese Datei von Hand auf, und eine neue Testfunktion
/// wird hier eingetragen.
///
/// setUp und tearDown gehoeren hierher und nicht in die Testdateien: Unity ruft
/// sie um jeden einzelnen Test, und es darf sie je Programm nur einmal geben.
/// Leer, solange kein Test etwas aufzubauen hat.
///

void test_codec_kennt_die_wire_nummern(void);
void test_codec_reicht_den_pfad_mit_startfeld_durch(void);
void test_codec_weist_pfade_ohne_weg_ab(void);


void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_codec_kennt_die_wire_nummern);
    RUN_TEST(test_codec_reicht_den_pfad_mit_startfeld_durch);
    RUN_TEST(test_codec_weist_pfade_ohne_weg_ab);

    return UNITY_END();
}
