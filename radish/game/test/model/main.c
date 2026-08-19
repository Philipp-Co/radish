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

    return UNITY_END();
}
