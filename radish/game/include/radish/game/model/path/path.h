#ifndef __RAD_GAME_PATH_H__
#define __RAD_GAME_PATH_H__

#include <stdint.h>

///
/// Ein Weg ueber das Raster: die Felder, auf denen eine Figur der Reihe nach
/// steht -- vom ersten, auf dem sie schon steht, bis zum letzten, auf dem sie
/// ankommt.
///
/// **Er steht im Modell, weil beide Grenzen des Spiels ihn sprechen.** Ein Pfad
/// kommt herein -- als Bewegung in einem Kommando (control/command/command.h) --
/// und er kommt heraus, sobald sie ausgefuehrt ist (RAD_OnEntityMoved_t in
/// control/events/event_manager.h). Waere er auf jeder Seite eigens erklaert,
/// gaebe es zwei fast gleiche Typen, die auseinanderlaufen, sobald einer sich
/// aendert, und dazwischen eine Kopierschleife, die nichts uebersetzt. Ein Name,
/// eine Quelle -- dieselbe Ueberlegung wie bei RAD_Tile_t und RAD_Entity_t
/// (model.h).
///
/// Damit ist ein Pfad Vokabular und kein Zustand: wer einen hinschreibt, braucht
/// keine Welt. Er liegt deshalb im oeffentlichen Baum, waehrend die Welt, in der
/// er gelaufen wird, privat bleibt.
///
/// **Mit Startfeld.** steps_to[0] ist das Feld, auf dem die Figur schon steht,
/// steps_to[number_of_steps-1] das, auf dem sie ankommt. Ein Pfad ist damit die
/// vollstaendige Folge ihrer Standorte und nicht die Folge ihrer Schritte -- bei n
/// Feldern sind es n-1 Schritte.
///
/// **Das war einmal umgekehrt, und der Grund fuer die Umstellung ist der Weg
/// hinaus.** Ein Pfad ohne Startfeld ist beim Hereinkommen bequemer -- wo eine
/// Figur steht, weiss sie selbst. Beim Herausgehen ist er es nicht: das Ereignis
/// kommt, nachdem die Figur schon am Ziel steht (RAD_OnEntityMoved_t), also nennen
/// entity->x/y und der letzte Schritt dasselbe Feld, und woher sie kam, steht
/// nirgends. Wer zeichnet, muss aber genau das wissen -- er hat eine Figur von
/// einem Feld auf ein anderes umzuhaengen. Ohne Startfeld bliebe ihm nur eine
/// eigene Buchfuehrung darueber, wo er sie zuletzt gesehen hat.
///
/// **Es wird nicht dagegen geprueft.** Dass in steps_to[0] wirklich steht, wo die
/// Figur ist, verlangt niemand: ein Kommando darf zweimal zugestellt werden und
/// muss dann zum selben Ergebnis fuehren (command.h). Beim zweiten Mal steht die
/// Figur schon am Ziel, und ein Vergleich mit steps_to[0] waere ein Fehlschlag, wo
/// keiner sein soll. Das Startfeld ist eine Angabe fuer den, der zusieht, und
/// keine Bedingung fuer den, der ausfuehrt.
///
/// **Weltkoordinaten, keine Verschiebungen.** Jeder Schritt benennt sein Feld
/// absolut. Das ist die Festlegung, die bisher fuer das eine Ziel von
/// move_entity galt, jetzt je Schritt: eine Folge von Verschiebungen bedeutet an
/// jeder Position etwas anderes, eine Folge von Feldern ueberall dasselbe.
///
/// **Ein festes Feld und kein Zeiger.** Ein Kommando ist Daten, die sich
/// kopieren, puffern und verschicken lassen (command.h); ein Pfad daran als
/// Zeiger waere keines von dreien, und der Codec muesste allokieren. Der Preis
/// ist eine Obergrenze -- und die ist gewollt.
///
/// **Die Laenge des Feldes ist die Obergrenze einer Bewegung.** Wie weit eine
/// Figur in einem Zug kommt, ist eine Festlegung der Regeln und keine des
/// Programms, das sie haelt: mehr als RAD_PATH_MAX_STEPS Felder kann ein Weg
/// nicht haben, weil kein Weg mehr tragen kann. Sechzehn sind auf einer
/// 8x8-Welt reichlich. Wer sie aendert, aendert das Uebertragungsformat mit --
/// die Schritte fahren alle mit, auch die ungenutzten (move_entity.h).
///
/// **Sechzehn Felder sind fuenfzehn Schritte**, seit das Startfeld mitfaehrt. Die
/// Zahl ist bewusst nicht auf siebzehn erhoeht worden, um die alte Reichweite zu
/// halten: das haette die Nutzlast von move_entity um vier Byte verlaengert und
/// damit das Uebertragungsformat gebrochen, an dem beide Seiten haengen. Ein Feld
/// weniger weit zu kommen ist auf 8x8 keine Einschraenkung, die jemand merkt --
/// ein Weg ueber fuenfzehn Felder besucht fast ein Viertel der Welt.
///
#define RAD_PATH_MAX_STEPS 16

///
/// Ein Feld im Raster. Dieselben zwei int16 wie in RAD_Tile_t und RAD_Entity_t:
/// ein Pfad zeigt auf Tiles und rechnet nicht in einer eigenen Einheit.
///
typedef struct
{
    int16_t x;
    int16_t y;
} RAD_EntityPosition_t;

///
/// Der Pfad: die Felder in ihrer Reihenfolge und wie viele es sind.
///
/// "number_of_steps" gilt in [2, RAD_PATH_MAX_STEPS] und zaehlt Felder, nicht
/// Schritte -- der Name ist aelter als das Startfeld und bleibt, weil er ueber die
/// Strecke geht.
///
/// **Ein Feld ist kein Pfad.** Null bedeutet die Abwesenheit einer Bewegung -- wer
/// nichts tut, schickt kein Kommando und veroeffentlicht kein Ereignis --, und eins
/// bedeutet dasselbe: es nennt nur, wo die Figur schon steht. Erst ab zwei ist ein
/// Weg beschrieben.
///
/// Die Plaetze hinter dem Zaehler sind genullt und bedeuten nichts; wer einen Pfad
/// bekommt, darf sich auf sie nicht berufen.
///
/// int8_t und nicht uint8_t: ein Byte ist es in beiden Faellen, und so stand der
/// Zaehler schon, als der Pfad nur der Ereignisseite gehoerte. Der Codec liest
/// ihn als Zweierkomplement und weist alles ausserhalb der Grenzen ab -- eine
/// negative Anzahl faellt damit auf wie eine zu grosse.
///
typedef struct
{
    RAD_EntityPosition_t steps_to[RAD_PATH_MAX_STEPS];
    int8_t number_of_steps;
} RAD_EntityPath_t;

#endif
