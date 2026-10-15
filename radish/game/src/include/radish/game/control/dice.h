#ifndef __RAD_GAME_CONTROL_DICE_H__
#define __RAD_GAME_CONTROL_DICE_H__

#include <stdint.h>

///
/// Das Wuerfeln der Spielregeln: ein Zufallsgenerator mit eigenem Zustand (PCG32,
/// siehe pcg-random.org) und darauf die Wuerfe.
///
/// **Nicht rand().** rand() haelt einen Zustand fuer den ganzen Prozess -- zwei
/// Spiele oder zwei Tests zoegen aus derselben Folge --, und Algorithmus und
/// RAND_MAX bringt die libc mit: unter Emscripten kaeme zum selben Seed eine andere
/// Folge heraus als auf dem Host. Hier liefert derselbe Seed auf jeder Plattform
/// dieselbe Folge. Damit kann ein Test feste Wuerfe erwarten, und ein Spiel laesst
/// sich aus Seed und ausgefuehrten Kommandos nachspielen.
///
/// **Den Seed besorgt sich das Spiel selbst** (RAD_DiceFreshSeed, gerufen in
/// RAD_CreateGame). Von aussen kommt keiner herein, und keiner geht hinaus.
///
/// Kryptografisch sicher ist PCG32 nicht. Das muss es nicht sein, solange der
/// Seed das Spiel nicht verlaesst -- wer ihn kennt, kennt alle Wuerfe.
///

typedef struct
{
    uint64_t state;
} RAD_Dice_t;

///
/// Ein Seed, der sich von Aufruf zu Aufruf unterscheidet -- nur aus Standard-C,
/// denn radish_game darf nichts aus der Umgebung benutzen (kein POSIX, kein
/// Emscripten), also weder getentropy() noch /dev/urandom.
///
/// Gemischt werden vier Quellen, von denen keine allein reicht:
///
///     time(NULL)   verschiedene Prozesse      nur auf die Sekunde genau
///     clock()      verbrauchte Rechenzeit     am Prozessanfang oft 0
///     "salt"       gleichzeitige Spiele, mit  nach free kann dieselbe
///                  ASLR auch Prozesse         Adresse wiederkommen
///     Zaehler      jeder Aufruf im Prozess    unterscheidet keine Prozesse
///
/// Der Zaehler ist der einzige statische Zustand des Moduls. Er garantiert, dass
/// zwei Aufrufe im selben Prozess verschiedene Seeds geben, auch in derselben
/// Sekunde mit demselben "salt" (anlegen, freigeben, wieder anlegen). Thread-
/// sicher ist er nicht; Spiele werden aus einem Thread angelegt.
///
/// Zwei Prozesse, die in derselben Sekunde ohne ASLR starten, koennen denselben
/// Seed bekommen. Fuer ein Spiel ist das vertretbar.
///
uint64_t RAD_DiceFreshSeed(const void *salt);

///
/// Setzt den Generator auf den Anfang der Folge zu "seed". Jeder Wert ist ein
/// gueltiger Seed, auch 0.
///
void RAD_DiceSeed(RAD_Dice_t *dice, uint64_t seed);

///
/// Die naechste Zahl der Folge, gleichverteilt in [0, 2^32).
///
uint32_t RAD_DiceNext(RAD_Dice_t *dice);

///
/// Ein Wurf mit einem Wuerfel mit "sides" Seiten: gleichverteilt in [1, sides].
/// Liefert 0, ohne zu ziehen, wenn "sides" kleiner als 1 ist.
///
int32_t RAD_DiceRoll(RAD_Dice_t *dice, int32_t sides);

///
/// Ein Wurf mit dem W10: gleichverteilt in [1, 10].
///
int32_t RAD_DiceD10(RAD_Dice_t *dice);

#endif
