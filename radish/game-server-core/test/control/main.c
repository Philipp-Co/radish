#include <unity.h>

///
/// Die Tests der Server-Steuerung (src/control/). Wie in game/test: die Prototypen
/// und RUN_TEST stehen hier von Hand, Unity laeuft ohne den Ruby-Generator.
///
/// setUp und tearDown gehoeren hierher und nicht in die Testdateien: Unity ruft
/// sie fuer jeden Test, und es darf sie im Programm nur einmal geben.
///

void test_spielstart_traegt_spieler_und_armeen_ein(void);
void test_spielstart_geht_nur_einmal(void);
void test_spielstart_mit_ungueltiger_kennung_aendert_nichts(void);
void test_spielstart_aus_datei_ueber_die_steuerung(void);
void test_spielstart_ohne_datei_ueber_die_steuerung(void);

void test_deploy_ueber_die_steuerung_stellt_auf(void);
void test_deploy_ueber_die_steuerung_nennt_den_grund(void);
void test_deploy_steuerung_zeigt_die_einheiten(void);
void test_deploy_meldet_die_aufgestellte_einheit(void);
void test_ziehen_und_angreifen_ueber_die_steuerung_nennt_den_grund(void);
void test_nach_dem_ende_nimmt_die_steuerung_nichts_mehr_an(void);

void test_nach_dem_zuruecksetzen_geht_ein_zweiter_start(void);
void test_nach_dem_zuruecksetzen_kommen_die_ereignisse_an(void);
void test_ein_beendetes_spiel_bleibt_ohne_neustart_stehen(void);
void test_scheitert_das_neuanlegen_ist_nichts_mehr_da(void);

void test_zeitmessung_ohne_nachricht_ist_alles_null(void);
void test_zeitmessung_merkt_hoechstwert_und_durchschnitt(void);
void test_zeitmessung_rundet_den_durchschnitt_ab(void);
void test_zeitmessung_uhr_laeuft_nicht_rueckwaerts(void);


void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_spielstart_traegt_spieler_und_armeen_ein);
    RUN_TEST(test_spielstart_geht_nur_einmal);
    RUN_TEST(test_spielstart_mit_ungueltiger_kennung_aendert_nichts);
    RUN_TEST(test_spielstart_aus_datei_ueber_die_steuerung);
    RUN_TEST(test_spielstart_ohne_datei_ueber_die_steuerung);

    RUN_TEST(test_deploy_ueber_die_steuerung_stellt_auf);
    RUN_TEST(test_deploy_ueber_die_steuerung_nennt_den_grund);
    RUN_TEST(test_deploy_steuerung_zeigt_die_einheiten);
    RUN_TEST(test_deploy_meldet_die_aufgestellte_einheit);
    RUN_TEST(test_ziehen_und_angreifen_ueber_die_steuerung_nennt_den_grund);
    RUN_TEST(test_nach_dem_ende_nimmt_die_steuerung_nichts_mehr_an);

    RUN_TEST(test_nach_dem_zuruecksetzen_geht_ein_zweiter_start);
    RUN_TEST(test_nach_dem_zuruecksetzen_kommen_die_ereignisse_an);
    RUN_TEST(test_ein_beendetes_spiel_bleibt_ohne_neustart_stehen);
    RUN_TEST(test_scheitert_das_neuanlegen_ist_nichts_mehr_da);

    RUN_TEST(test_zeitmessung_ohne_nachricht_ist_alles_null);
    RUN_TEST(test_zeitmessung_merkt_hoechstwert_und_durchschnitt);
    RUN_TEST(test_zeitmessung_rundet_den_durchschnitt_ab);
    RUN_TEST(test_zeitmessung_uhr_laeuft_nicht_rueckwaerts);

    return UNITY_END();
}
