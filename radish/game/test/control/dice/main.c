#include <unity.h>

///
/// Der Runner fuer control/dice. Unity ruft die Tests nicht selbst auf -- ohne den
/// Ruby-Generator zaehlt sie diese Datei von Hand auf, und eine neue Testfunktion
/// wird hier eingetragen.
///
/// setUp und tearDown gehoeren hierher und nicht in die Testdateien: Unity ruft
/// sie um jeden einzelnen Test, und es darf sie je Programm nur einmal geben.
/// Leer, solange kein Test etwas aufzubauen hat.
///

void test_d10_bleibt_zwischen_eins_und_zehn(void);
void test_d10_ist_gleichverteilt(void);
void test_derselbe_seed_gibt_dieselbe_folge(void);
void test_ein_anderer_seed_gibt_eine_andere_folge(void);
void test_frische_seeds_unterscheiden_sich_auch_mit_demselben_salz(void);
void test_ein_wuerfel_mit_einer_seite_zeigt_immer_eins(void);
void test_ohne_seiten_wird_nicht_gewuerfelt(void);
void test_bei_vielen_verworfenen_werten_bleibt_der_wurf_im_bereich(void);


void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_d10_bleibt_zwischen_eins_und_zehn);
    RUN_TEST(test_d10_ist_gleichverteilt);
    RUN_TEST(test_derselbe_seed_gibt_dieselbe_folge);
    RUN_TEST(test_ein_anderer_seed_gibt_eine_andere_folge);
    RUN_TEST(test_frische_seeds_unterscheiden_sich_auch_mit_demselben_salz);
    RUN_TEST(test_ein_wuerfel_mit_einer_seite_zeigt_immer_eins);
    RUN_TEST(test_ohne_seiten_wird_nicht_gewuerfelt);
    RUN_TEST(test_bei_vielen_verworfenen_werten_bleibt_der_wurf_im_bereich);

    return UNITY_END();
}
