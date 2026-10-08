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

    return UNITY_END();
}
