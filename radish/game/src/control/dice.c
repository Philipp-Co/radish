#include <radish/game/control/dice.h>
#include <time.h>

///
/// Die Konstanten von PCG32: der Multiplikator des linearen Kongruenzgenerators
/// und das Inkrement des Standardstroms. Das Inkrement muss ungerade sein.
///
#define RAD_DICE_MULTIPLIER 6364136223846793005ULL
#define RAD_DICE_INCREMENT 1442695040888963407ULL

static uint64_t RAD_DiceMix(uint64_t value);

///
/// Wie viele Seeds dieser Prozess schon gebildet hat (RAD_DiceFreshSeed).
///
static uint64_t RAD_dice_seeds_made = 0;

uint64_t RAD_DiceFreshSeed(const void *salt)
{
    // Jede Quelle wird einzeln gemischt, bevor die naechste dazukommt: sonst
    // koennten sich zwei Quellen, die sich zugleich aendern, beim XOR aufheben.
    uint64_t seed = RAD_DiceMix((uint64_t)time(NULL));
    seed = RAD_DiceMix(seed ^ (uint64_t)clock());
    seed = RAD_DiceMix(seed ^ (uint64_t)(uintptr_t)salt);
    seed = RAD_DiceMix(seed ^ ++RAD_dice_seeds_made);
    return seed;
}

void RAD_DiceSeed(RAD_Dice_t *dice, uint64_t seed)
{
    // Wie pcg32_srandom_r: der Seed geht erst nach einem Schritt hinein und wird
    // dann noch einmal durchgemischt, damit nahe Seeds nicht nahe Folgen geben.
    dice->state = 0;
    RAD_DiceNext(dice);
    dice->state += seed;
    RAD_DiceNext(dice);
}

uint32_t RAD_DiceNext(RAD_Dice_t *dice)
{
    const uint64_t old = dice->state;
    dice->state = old * RAD_DICE_MULTIPLIER + RAD_DICE_INCREMENT;

    // Ausgegeben wird nicht der Zustand selbst, sondern seine oberen Bits,
    // verschoben und um einen Betrag aus den obersten fuenf Bits rotiert.
    const uint32_t xorshifted = (uint32_t)(((old >> 18u) ^ old) >> 27u);
    const uint32_t rotation = (uint32_t)(old >> 59u);
    return (xorshifted >> rotation) | (xorshifted << ((0u - rotation) & 31u));
}

int32_t RAD_DiceRoll(RAD_Dice_t *dice, int32_t sides)
{
    if(sides < 1)
    {
        return 0;
    }

    // 2^32 ist im Allgemeinen kein Vielfaches von "sides": mit blossem % kaemen die
    // kleinen Augen oefter. Die untersten 2^32 mod sides Werte werden deshalb
    // verworfen und neu gezogen -- (0 - sides) % sides ist genau diese Zahl. Beim
    // W10 sind das 6 von 2^32 Werten.
    const uint32_t range = (uint32_t)sides;
    const uint32_t threshold = (0u - range) % range;
    uint32_t value;
    do
    {
        value = RAD_DiceNext(dice);
    } while(value < threshold);

    return (int32_t)(value % range) + 1;
}

int32_t RAD_DiceD10(RAD_Dice_t *dice)
{
    return RAD_DiceRoll(dice, 10);
}

///
/// Der Finalizer von SplitMix64: aendert sich ein Bit der Eingabe, aendert sich
/// im Mittel die Haelfte der Ausgabe. Damit ergeben auch Quellen, die sich nur in
/// den untersten Bits unterscheiden (Zaehler, Sekunden), weit entfernte Seeds.
///
static uint64_t RAD_DiceMix(uint64_t value)
{
    value += 0x9e3779b97f4a7c15ULL;
    value = (value ^ (value >> 30u)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27u)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31u);
}
