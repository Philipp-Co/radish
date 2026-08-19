#include <unity.h>

///
/// Der Runner fuer control/execute. Unity ruft die Tests nicht selbst auf -- ohne den
/// Ruby-Generator zaehlt sie diese Datei von Hand auf, und eine neue Testfunktion
/// wird hier eingetragen.
///
/// setUp und tearDown gehoeren hierher und nicht in die Testdateien: Unity ruft
/// sie um jeden einzelnen Test, und es darf sie je Programm nur einmal geben.
/// Leer, solange kein Test etwas aufzubauen hat.
///

void test_fabrik_fuellt_den_kopf(void);


void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_fabrik_fuellt_den_kopf);

    return UNITY_END();
}
