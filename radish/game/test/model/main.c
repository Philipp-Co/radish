#include <unity.h>

///
/// Der Runner fuer model. Unity ruft die Tests nicht selbst auf -- ohne den
/// Ruby-Generator zaehlt sie diese Datei von Hand auf, und eine neue Testfunktion
/// wird hier eingetragen.
///
/// setUp und tearDown gehoeren hierher und nicht in die Testdateien: Unity ruft
/// sie um jeden einzelnen Test, und es darf sie je Programm nur einmal geben.
/// Leer, solange kein Test etwas aufzubauen hat.
///

void test_turn_faengt_leer_an(void);
void test_world_faengt_leer_und_stimmig_an(void);
void test_world_tile_hinzufuegen_setzt_typ_und_hoehe(void);
void test_world_tile_entfernen_macht_void_und_laesst_die_stelle_stehen(void);
void test_world_tile_entfernen_scheitert_unter_einer_figur(void);
void test_world_tile_ausserhalb_der_welt_ist_kein_tile(void);
void test_world_tile_ereignis_folgt_dem_uebergang(void);
void test_world_tile_entfernen_ist_idempotent(void);
void test_world_init_meldet_jedes_feld_einmal(void);
void test_world_aenderungen_gegen_alten_stand(void);
void test_world_groesse_laesst_sich_setzen(void);
void test_world_ungueltige_groesse_aendert_nichts(void);
void test_world_aenderungen_ueber_eine_neue_groesse(void);

void test_world_reserve_einheit_steht_auf_keinem_feld(void);
void test_world_reserve_lehnt_unhaltbare_werte_ab(void);
void test_world_reserve_lehnt_ab_wenn_der_pool_voll_ist(void);
void test_world_einheit_aus_der_reserve_aufstellen(void);
void test_world_aufstellen_wird_abgelehnt(void);
void test_world_nur_einheiten_auf_dem_feld_ziehen(void);
void test_world_entfernen_zerstoert_und_vergibt_die_id_nicht_neu(void);
void test_world_konsistenz_kennt_die_zustaende(void);


void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_turn_faengt_leer_an);
    RUN_TEST(test_world_faengt_leer_und_stimmig_an);
    RUN_TEST(test_world_tile_hinzufuegen_setzt_typ_und_hoehe);
    RUN_TEST(test_world_tile_entfernen_macht_void_und_laesst_die_stelle_stehen);
    RUN_TEST(test_world_tile_entfernen_scheitert_unter_einer_figur);
    RUN_TEST(test_world_tile_ausserhalb_der_welt_ist_kein_tile);
    RUN_TEST(test_world_tile_ereignis_folgt_dem_uebergang);
    RUN_TEST(test_world_tile_entfernen_ist_idempotent);
    RUN_TEST(test_world_init_meldet_jedes_feld_einmal);
    RUN_TEST(test_world_aenderungen_gegen_alten_stand);
    RUN_TEST(test_world_groesse_laesst_sich_setzen);
    RUN_TEST(test_world_ungueltige_groesse_aendert_nichts);
    RUN_TEST(test_world_aenderungen_ueber_eine_neue_groesse);

    RUN_TEST(test_world_reserve_einheit_steht_auf_keinem_feld);
    RUN_TEST(test_world_reserve_lehnt_unhaltbare_werte_ab);
    RUN_TEST(test_world_reserve_lehnt_ab_wenn_der_pool_voll_ist);
    RUN_TEST(test_world_einheit_aus_der_reserve_aufstellen);
    RUN_TEST(test_world_aufstellen_wird_abgelehnt);
    RUN_TEST(test_world_nur_einheiten_auf_dem_feld_ziehen);
    RUN_TEST(test_world_entfernen_zerstoert_und_vergibt_die_id_nicht_neu);
    RUN_TEST(test_world_konsistenz_kennt_die_zustaende);

    return UNITY_END();
}
