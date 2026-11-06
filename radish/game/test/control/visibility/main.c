#include <unity.h>

///
/// Der Runner fuer control/visibility. Unity ruft die Tests nicht selbst auf --
/// ohne den Ruby-Generator zaehlt sie diese Datei von Hand auf, und eine neue
/// Testfunktion wird hier eingetragen.
///
/// setUp und tearDown gehoeren hierher und nicht in die Testdateien: Unity ruft
/// sie um jeden einzelnen Test, und es darf sie je Programm nur einmal geben.
/// Leer -- jeder Test baut sein Spiel selbst auf und ab.
///

void test_sicht_reicht_genau_bis_zur_sichtweite(void);
void test_sichtweite_null_sieht_nur_das_eigene_feld(void);
void test_gelaende_versperrt_was_dahinter_liegt(void);
void test_einheit_versperrt_was_dahinter_liegt_und_wird_selbst_gesehen(void);
void test_durch_eine_diagonale_luecke_sieht_man(void);
void test_reserve_und_zerstoerte_sehen_nichts(void);
void test_eigene_einheiten_ergaenzen_sich_fremde_zaehlen_nicht(void);
void test_ohne_besitzer_sieht_niemand(void);
void test_eigene_einheit_immer_sichtbar_fremde_nur_in_sicht(void);
void test_raster_stimmt_mit_der_einzelabfrage_ueberein(void);
void test_ungueltiges_sieht_nichts(void);
void test_empfaenger_eines_feldes(void);
void test_sichtbarer_teil_eines_weges(void);
void test_beobachteter_teil_eines_weges(void);

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_sicht_reicht_genau_bis_zur_sichtweite);
    RUN_TEST(test_sichtweite_null_sieht_nur_das_eigene_feld);
    RUN_TEST(test_gelaende_versperrt_was_dahinter_liegt);
    RUN_TEST(test_einheit_versperrt_was_dahinter_liegt_und_wird_selbst_gesehen);
    RUN_TEST(test_durch_eine_diagonale_luecke_sieht_man);
    RUN_TEST(test_reserve_und_zerstoerte_sehen_nichts);
    RUN_TEST(test_eigene_einheiten_ergaenzen_sich_fremde_zaehlen_nicht);
    RUN_TEST(test_ohne_besitzer_sieht_niemand);
    RUN_TEST(test_eigene_einheit_immer_sichtbar_fremde_nur_in_sicht);
    RUN_TEST(test_raster_stimmt_mit_der_einzelabfrage_ueberein);
    RUN_TEST(test_ungueltiges_sieht_nichts);
    RUN_TEST(test_empfaenger_eines_feldes);
    RUN_TEST(test_sichtbarer_teil_eines_weges);
    RUN_TEST(test_beobachteter_teil_eines_weges);

    return UNITY_END();
}
