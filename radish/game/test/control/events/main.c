#include <unity.h>

///
/// Der Runner fuer control/events. Unity ruft die Tests nicht selbst auf -- ohne den
/// Ruby-Generator zaehlt sie diese Datei von Hand auf, und eine neue Testfunktion
/// wird hier eingetragen.
///
/// setUp und tearDown gehoeren hierher und nicht in die Testdateien: Unity ruft
/// sie um jeden einzelnen Test, und es darf sie je Programm nur einmal geben.
/// Leer, solange kein Test etwas aufzubauen hat.
///

void test_manager_reicht_ein_ereignis_durch(void);


void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_manager_reicht_ein_ereignis_durch);

    return UNITY_END();
}
