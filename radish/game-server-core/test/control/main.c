#include <unity.h>

///
/// Die Tests der Server-Steuerung (src/control/). Wie in game/test: die Prototypen
/// und RUN_TEST stehen hier von Hand, Unity laeuft ohne den Ruby-Generator.
///
/// setUp und tearDown gehoeren hierher und nicht in die Testdateien: Unity ruft
/// sie fuer jeden Test, und es darf sie im Programm nur einmal geben.
///

void test_spielstart_liest_beide_spieler(void);
void test_spielstart_uebergeht_unbekannte_schluessel(void);
void test_spielstart_lehnt_fehlerhafte_dateien_ab(void);
void test_spielstart_fehler_laesst_stand_unberuehrt(void);
void test_spielstart_fehlende_datei(void);
void test_spielstart_aus_datei(void);
void test_spielstart_liest_die_werte_der_einheiten(void);
void test_spielstart_lehnt_ab_was_das_spiel_nicht_halten_kann(void);
void test_spielstart_kennung_wird_zur_id(void);

void test_spielstart_traegt_spieler_und_armeen_ein(void);
void test_spielstart_geht_nur_einmal(void);
void test_spielstart_mit_ungueltiger_kennung_aendert_nichts(void);

void test_deploy_ueber_die_steuerung_stellt_auf(void);
void test_deploy_ueber_die_steuerung_nennt_den_grund(void);
void test_deploy_steuerung_zeigt_die_einheiten(void);


void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_spielstart_liest_beide_spieler);
    RUN_TEST(test_spielstart_uebergeht_unbekannte_schluessel);
    RUN_TEST(test_spielstart_lehnt_fehlerhafte_dateien_ab);
    RUN_TEST(test_spielstart_fehler_laesst_stand_unberuehrt);
    RUN_TEST(test_spielstart_fehlende_datei);
    RUN_TEST(test_spielstart_aus_datei);
    RUN_TEST(test_spielstart_liest_die_werte_der_einheiten);
    RUN_TEST(test_spielstart_lehnt_ab_was_das_spiel_nicht_halten_kann);
    RUN_TEST(test_spielstart_kennung_wird_zur_id);

    RUN_TEST(test_spielstart_traegt_spieler_und_armeen_ein);
    RUN_TEST(test_spielstart_geht_nur_einmal);
    RUN_TEST(test_spielstart_mit_ungueltiger_kennung_aendert_nichts);

    RUN_TEST(test_deploy_ueber_die_steuerung_stellt_auf);
    RUN_TEST(test_deploy_ueber_die_steuerung_nennt_den_grund);
    RUN_TEST(test_deploy_steuerung_zeigt_die_einheiten);

    return UNITY_END();
}
