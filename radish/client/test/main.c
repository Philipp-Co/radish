#include <unity.h>

///
/// Der Runner fuer die Host-Tests des Clients (client/test/CMakeLists.txt). Die
/// Prototypen und RUN_TEST stehen von Hand hier, wie in game/test -- Unity laeuft
/// ohne den Ruby-Generator.
///

void test_sprite_liest_den_panzer(void);
void test_sprite_nimmt_die_gueltigen_beispiele_an(void);
void test_sprite_weist_die_ungueltigen_beispiele_ab(void);
void test_sprite_prueft_die_schluesselbilder(void);
void test_sprite_grenzen_und_fehler_lassen_die_ausgabe_stehen(void);
void test_sprite_atlanten_je_animation_und_richtung(void);
void test_sprite_schluessel_und_pfad(void);
void test_sprite_bilder_einer_folge(void);

void setUp(void) {}
void tearDown(void) {}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_sprite_liest_den_panzer);
    RUN_TEST(test_sprite_nimmt_die_gueltigen_beispiele_an);
    RUN_TEST(test_sprite_weist_die_ungueltigen_beispiele_ab);
    RUN_TEST(test_sprite_prueft_die_schluesselbilder);
    RUN_TEST(test_sprite_grenzen_und_fehler_lassen_die_ausgabe_stehen);
    RUN_TEST(test_sprite_atlanten_je_animation_und_richtung);
    RUN_TEST(test_sprite_schluessel_und_pfad);
    RUN_TEST(test_sprite_bilder_einer_folge);
    return UNITY_END();
}
