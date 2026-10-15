#include <unity.h>
#include <radish/game/control/dice.h>

///
/// Wie viele Wuerfe die Tests ueber die Verteilung machen. Genug, dass jedes Auge
/// rund 10 000 Mal faellt.
///
#define RAD_TEST_ROLLS 100000

void test_d10_bleibt_zwischen_eins_und_zehn(void)
{
    RAD_Dice_t dice;
    RAD_DiceSeed(&dice, 42);

    for(int32_t i = 0; i < RAD_TEST_ROLLS; ++i)
    {
        const int32_t roll = RAD_DiceD10(&dice);
        TEST_ASSERT_GREATER_OR_EQUAL_INT32(1, roll);
        TEST_ASSERT_LESS_OR_EQUAL_INT32(10, roll);
    }
}

///
/// Chi-Quadrat-Test ueber die zehn Augen. 27,88 ist der Grenzwert bei 9
/// Freiheitsgraden fuer p = 0,001: eine echte Gleichverteilung laege nur in einem
/// von tausend Faellen darueber. Mit festem Seed ist der Test trotzdem
/// deterministisch -- er schlaegt nicht zufaellig fehl, sondern nur, wenn sich
/// die Folge aendert.
///
void test_d10_ist_gleichverteilt(void)
{
    RAD_Dice_t dice;
    RAD_DiceSeed(&dice, 42);

    int32_t counts[10] = {0};
    for(int32_t i = 0; i < RAD_TEST_ROLLS; ++i)
    {
        counts[RAD_DiceD10(&dice) - 1]++;
    }

    const double expected = RAD_TEST_ROLLS / 10.0;
    double chi_square = 0.0;
    for(int32_t face = 0; face < 10; ++face)
    {
        // Jedes Auge muss ueberhaupt fallen, sonst ist es keine Verteilung ueber zehn.
        TEST_ASSERT_GREATER_THAN_INT32(0, counts[face]);

        const double difference = counts[face] - expected;
        chi_square += (difference * difference) / expected;
    }

    TEST_ASSERT_TRUE(chi_square < 27.88);
}

void test_derselbe_seed_gibt_dieselbe_folge(void)
{
    RAD_Dice_t first;
    RAD_Dice_t second;
    RAD_DiceSeed(&first, 4711);
    RAD_DiceSeed(&second, 4711);

    for(int32_t i = 0; i < 1000; ++i)
    {
        TEST_ASSERT_EQUAL_UINT32(RAD_DiceNext(&first), RAD_DiceNext(&second));
    }
}

void test_ein_anderer_seed_gibt_eine_andere_folge(void)
{
    RAD_Dice_t first;
    RAD_Dice_t second;
    RAD_DiceSeed(&first, 4711);
    RAD_DiceSeed(&second, 4712);

    // Einzelne Zahlen duerfen gleich sein, zehn hintereinander nicht.
    int32_t equal = 0;
    for(int32_t i = 0; i < 10; ++i)
    {
        if(RAD_DiceNext(&first) == RAD_DiceNext(&second))
        {
            ++equal;
        }
    }
    TEST_ASSERT_LESS_THAN_INT32(10, equal);
}

///
/// Derselbe "salt" in derselben Sekunde -- unterscheiden kann die beiden Aufrufe
/// nur noch der Zaehler, und das muss reichen.
///
void test_frische_seeds_unterscheiden_sich_auch_mit_demselben_salz(void)
{
    int marker = 0;
    const uint64_t first = RAD_DiceFreshSeed(&marker);
    const uint64_t second = RAD_DiceFreshSeed(&marker);
    TEST_ASSERT_NOT_EQUAL_UINT64(first, second);
}

void test_ein_wuerfel_mit_einer_seite_zeigt_immer_eins(void)
{
    RAD_Dice_t dice;
    RAD_DiceSeed(&dice, 0);

    for(int32_t i = 0; i < 100; ++i)
    {
        TEST_ASSERT_EQUAL_INT32(1, RAD_DiceRoll(&dice, 1));
    }
}

///
/// Ohne Seiten kein Wurf -- und der Generator darf dabei nicht weiterlaufen,
/// sonst verschoebe ein falscher Aufruf alle folgenden Wuerfe.
///
void test_ohne_seiten_wird_nicht_gewuerfelt(void)
{
    RAD_Dice_t dice;
    RAD_DiceSeed(&dice, 42);
    const uint64_t before = dice.state;

    TEST_ASSERT_EQUAL_INT32(0, RAD_DiceRoll(&dice, 0));
    TEST_ASSERT_EQUAL_INT32(0, RAD_DiceRoll(&dice, -3));
    TEST_ASSERT_EQUAL_UINT64(before, dice.state);
}

///
/// Bei 2^30 + 1 Seiten werden rund ein Viertel aller Werte verworfen
/// (2^32 mod (2^30 + 1) = 2^30 - 3). Die Schleife muss also oft neu ziehen, und
/// das Ergebnis muss trotzdem im Bereich liegen.
///
void test_bei_vielen_verworfenen_werten_bleibt_der_wurf_im_bereich(void)
{
    const int32_t sides = (1 << 30) + 1;
    RAD_Dice_t dice;
    RAD_DiceSeed(&dice, 42);

    for(int32_t i = 0; i < 10000; ++i)
    {
        const int32_t roll = RAD_DiceRoll(&dice, sides);
        TEST_ASSERT_GREATER_OR_EQUAL_INT32(1, roll);
        TEST_ASSERT_LESS_OR_EQUAL_INT32(sides, roll);
    }
}
