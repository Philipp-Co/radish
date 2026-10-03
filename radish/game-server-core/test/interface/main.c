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
void test_message_trennt_den_absender_ab(void);
void test_message_liest_die_leere_reserve_anfrage(void);


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
    RUN_TEST(test_message_trennt_den_absender_ab);
    RUN_TEST(test_message_liest_die_leere_reserve_anfrage);

    return UNITY_END();
}
