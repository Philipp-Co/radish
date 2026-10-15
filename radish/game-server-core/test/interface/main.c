#include <unity.h>

///
/// Die Tests der Aussengrenze des Servers (src/interface/). Wie in control/: die
/// Prototypen und RUN_TEST stehen hier von Hand, Unity laeuft ohne den
/// Ruby-Generator.
///

void test_message_liest_ein_deploy_kommando(void);
void test_message_lehnt_ein_feld_jenseits_von_int16_ab(void);
void test_message_packt_die_antwort_auf_ein_deploy(void);
void test_message_erkennt_die_reserve_anfrage(void);
void test_message_packt_eine_einheit_der_reserve(void);
void test_message_packt_eine_aufgestellte_einheit(void);
void test_message_trennt_den_absender_ab(void);
void test_message_liest_die_leere_reserve_anfrage(void);
void test_message_liest_ein_end_turn_kommando(void);
void test_message_packt_die_antwort_auf_ein_end_turn(void);
void test_message_erkennt_die_einheiten_anfrage(void);
void test_message_packt_eine_einheit(void);
void test_message_packt_die_ganze_einheit(void);
void test_message_liest_ein_attack_kommando(void);
void test_message_lehnt_ein_angriffsziel_jenseits_von_int16_ab(void);
void test_message_packt_die_antwort_auf_ein_attack(void);
void test_message_packt_einen_zug(void);
void test_message_packt_einen_angriff(void);
void test_message_packt_einen_angriff_ohne_ziel(void);
void test_message_packt_eine_zerstoerung(void);
void test_message_packt_eine_aufstellzone(void);
void test_message_packt_die_runde(void);
void test_message_packt_das_ende(void);
void test_message_packt_das_siegfeld(void);
void test_message_packt_das_gelaende(void);


void test_report_sieger_und_verlierer_als_kennung(void);
void test_report_unentschieden_nennt_niemanden(void);
void test_report_ohne_zwei_mitspieler_gibt_es_keinen(void);
void test_report_startet_das_programm_mit_sieger_und_verlierer(void);
void test_report_startet_das_programm_bei_unentschieden_mit_draw(void);
void test_report_mit_einem_programm_das_es_nicht_gibt(void);

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();

    RUN_TEST(test_message_liest_ein_deploy_kommando);
    RUN_TEST(test_message_lehnt_ein_feld_jenseits_von_int16_ab);
    RUN_TEST(test_message_packt_die_antwort_auf_ein_deploy);
    RUN_TEST(test_message_erkennt_die_reserve_anfrage);
    RUN_TEST(test_message_packt_eine_einheit_der_reserve);
    RUN_TEST(test_message_packt_eine_aufgestellte_einheit);
    RUN_TEST(test_message_trennt_den_absender_ab);
    RUN_TEST(test_message_liest_die_leere_reserve_anfrage);
    RUN_TEST(test_message_liest_ein_end_turn_kommando);
    RUN_TEST(test_message_packt_die_antwort_auf_ein_end_turn);
    RUN_TEST(test_message_erkennt_die_einheiten_anfrage);
    RUN_TEST(test_message_packt_eine_einheit);
    RUN_TEST(test_message_packt_die_ganze_einheit);
    RUN_TEST(test_message_liest_ein_attack_kommando);
    RUN_TEST(test_message_lehnt_ein_angriffsziel_jenseits_von_int16_ab);
    RUN_TEST(test_message_packt_die_antwort_auf_ein_attack);
    RUN_TEST(test_message_packt_einen_zug);
    RUN_TEST(test_message_packt_einen_angriff);
    RUN_TEST(test_message_packt_einen_angriff_ohne_ziel);
    RUN_TEST(test_message_packt_eine_zerstoerung);
    RUN_TEST(test_message_packt_eine_aufstellzone);
    RUN_TEST(test_message_packt_die_runde);
    RUN_TEST(test_message_packt_das_ende);
    RUN_TEST(test_message_packt_das_siegfeld);
    RUN_TEST(test_message_packt_das_gelaende);


    RUN_TEST(test_report_sieger_und_verlierer_als_kennung);
    RUN_TEST(test_report_unentschieden_nennt_niemanden);
    RUN_TEST(test_report_ohne_zwei_mitspieler_gibt_es_keinen);
    RUN_TEST(test_report_startet_das_programm_mit_sieger_und_verlierer);
    RUN_TEST(test_report_startet_das_programm_bei_unentschieden_mit_draw);
    RUN_TEST(test_report_mit_einem_programm_das_es_nicht_gibt);

    return UNITY_END();
}
