#include <unity.h>

///
/// Der Runner fuer control/start_game: das Lesen der Spielstart-Datei
/// (test_parse.c, vorher im Server) und das Einrichten des Spiels danach
/// (test_start_game.c). Die Prototypen und RUN_TEST stehen von Hand hier, Unity
/// laeuft ohne den Ruby-Generator.
///

void test_spielstart_liest_beide_spieler(void);
void test_spielstart_liest_die_werte_der_einheiten(void);
void test_spielstart_uebergeht_unbekannte_schluessel(void);
void test_spielstart_lehnt_fehlerhafte_dateien_ab(void);
void test_spielstart_lehnt_ab_was_das_spiel_nicht_halten_kann(void);
void test_spielstart_fehler_laesst_stand_unberuehrt(void);
void test_spielstart_fehlende_datei(void);
void test_spielstart_aus_datei(void);
void test_spielstart_kennung_wird_zur_id(void);

void test_start_richtet_spieler_pool_und_reserven_ein(void);
void test_start_geht_nur_einmal(void);
void test_start_nimmt_vorab_beigetretene_mit(void);
void test_start_nimmt_nur_die_eigenen_beitritte_zurueck(void);
void test_start_mit_ungueltiger_kennung_aendert_nichts(void);
void test_start_lehnt_ab_was_nicht_in_den_pool_passt(void);
void test_start_ohne_spiel_oder_daten(void);
void test_start_aus_datei(void);
void test_start_aus_fehlender_datei_aendert_nichts(void);


void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_spielstart_liest_beide_spieler);
    RUN_TEST(test_spielstart_liest_die_werte_der_einheiten);
    RUN_TEST(test_spielstart_uebergeht_unbekannte_schluessel);
    RUN_TEST(test_spielstart_lehnt_fehlerhafte_dateien_ab);
    RUN_TEST(test_spielstart_lehnt_ab_was_das_spiel_nicht_halten_kann);
    RUN_TEST(test_spielstart_fehler_laesst_stand_unberuehrt);
    RUN_TEST(test_spielstart_fehlende_datei);
    RUN_TEST(test_spielstart_aus_datei);
    RUN_TEST(test_spielstart_kennung_wird_zur_id);

    RUN_TEST(test_start_richtet_spieler_pool_und_reserven_ein);
    RUN_TEST(test_start_geht_nur_einmal);
    RUN_TEST(test_start_nimmt_vorab_beigetretene_mit);
    RUN_TEST(test_start_nimmt_nur_die_eigenen_beitritte_zurueck);
    RUN_TEST(test_start_mit_ungueltiger_kennung_aendert_nichts);
    RUN_TEST(test_start_lehnt_ab_was_nicht_in_den_pool_passt);
    RUN_TEST(test_start_ohne_spiel_oder_daten);
    RUN_TEST(test_start_aus_datei);
    RUN_TEST(test_start_aus_fehlender_datei_aendert_nichts);

    return UNITY_END();
}
