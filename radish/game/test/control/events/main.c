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
void test_empfaengerliste_nimmt_jeden_einmal(void);
void test_ereignis_erreicht_nur_die_empfaenger(void);
void test_ohne_empfaenger_abonniert_bekommt_alles_einmal(void);
void test_abo_ersetzen_und_abmelden(void);
void test_volle_abo_tabelle_weist_ab(void);
void test_fehlende_callbacks_werden_voreingestellt(void);
void test_weitere_fassung_nur_an_die_liste(void);


void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_manager_reicht_ein_ereignis_durch);
    RUN_TEST(test_empfaengerliste_nimmt_jeden_einmal);
    RUN_TEST(test_ereignis_erreicht_nur_die_empfaenger);
    RUN_TEST(test_ohne_empfaenger_abonniert_bekommt_alles_einmal);
    RUN_TEST(test_abo_ersetzen_und_abmelden);
    RUN_TEST(test_volle_abo_tabelle_weist_ab);
    RUN_TEST(test_fehlende_callbacks_werden_voreingestellt);
    RUN_TEST(test_weitere_fassung_nur_an_die_liste);

    return UNITY_END();
}
