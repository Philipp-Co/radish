#include <unity.h>

///
/// Der Runner fuer game. Unity ruft die Tests nicht selbst auf -- ohne den
/// Ruby-Generator zaehlt sie diese Datei von Hand auf, und eine neue Testfunktion
/// wird hier eingetragen.
///
/// setUp und tearDown gehoeren hierher und nicht in die Testdateien: Unity ruft
/// sie um jeden einzelnen Test, und es darf sie je Programm nur einmal geben.
/// Leer, solange kein Test etwas aufzubauen hat.
///

void test_game_wird_leer_angelegt(void);
void test_game_move_kommando_veroeffentlicht_ein_ereignis(void);
void test_game_move_kommando_ohne_figur_meldet_ein_ergebnis(void);

void test_move_kommando_bewegt_die_figur(void);
void test_move_kommando_auf_besetztes_feld_bewegt_nicht(void);
void test_move_kommando_ueber_den_rand_bewegt_nicht(void);

void test_view_zaehlt_alle_tiles(void);
void test_view_liefert_jedes_tile_mit_seiner_position(void);
void test_view_weist_ungueltige_tile_zugriffe_ab(void);
void test_view_laesst_output_bei_ablehnung_unberuehrt(void);
void test_view_zaehlt_nur_vorhandene_entitaeten(void);
void test_view_index_ist_dicht_auch_bei_luecken_im_pool(void);
void test_view_gibt_eine_kopie_heraus(void);
void test_view_findet_die_gesetzte_figur_auf_ihrem_tile(void);

void test_tile_hinzufuegen_geht_durch_bis_in_die_welt(void);
void test_tile_entfernen_macht_void(void);
void test_tile_ohne_spiel_ist_kein_absturz(void);
void test_tile_ereignisse_kommen_beim_abonnenten_an(void);
void test_tile_ohne_aenderung_kommt_kein_ereignis_an(void);


void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_game_wird_leer_angelegt);
    RUN_TEST(test_game_move_kommando_veroeffentlicht_ein_ereignis);
    RUN_TEST(test_game_move_kommando_ohne_figur_meldet_ein_ergebnis);

    RUN_TEST(test_move_kommando_bewegt_die_figur);
    RUN_TEST(test_move_kommando_auf_besetztes_feld_bewegt_nicht);
    RUN_TEST(test_move_kommando_ueber_den_rand_bewegt_nicht);

    RUN_TEST(test_view_zaehlt_alle_tiles);
    RUN_TEST(test_view_liefert_jedes_tile_mit_seiner_position);
    RUN_TEST(test_view_weist_ungueltige_tile_zugriffe_ab);
    RUN_TEST(test_view_laesst_output_bei_ablehnung_unberuehrt);
    RUN_TEST(test_view_zaehlt_nur_vorhandene_entitaeten);
    RUN_TEST(test_view_index_ist_dicht_auch_bei_luecken_im_pool);
    RUN_TEST(test_view_gibt_eine_kopie_heraus);
    RUN_TEST(test_view_findet_die_gesetzte_figur_auf_ihrem_tile);

    RUN_TEST(test_tile_hinzufuegen_geht_durch_bis_in_die_welt);
    RUN_TEST(test_tile_entfernen_macht_void);
    RUN_TEST(test_tile_ohne_spiel_ist_kein_absturz);
    RUN_TEST(test_tile_ereignisse_kommen_beim_abonnenten_an);
    RUN_TEST(test_tile_ohne_aenderung_kommt_kein_ereignis_an);

    return UNITY_END();
}
