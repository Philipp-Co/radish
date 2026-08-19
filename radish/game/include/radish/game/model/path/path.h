#ifndef __RAD_GAME_PATH_H__
#define __RAD_GAME_PATH_H__

#include <stdint.h>

///
/// Ein Weg ueber das Raster: die Felder, die eine Figur der Reihe nach betritt.
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
/// **Ohne Startfeld.** Ein Pfad sagt, wohin es geht, und nicht, wo es losgeht --
/// wo eine Figur steht, weiss sie selbst (RAD_Entity_t.x/y). Der erste Schritt
/// ist das erste betretene Feld, nicht das verlassene.
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
/// Der Pfad: die betretenen Felder in ihrer Reihenfolge und wie viele es sind.
///
/// "number_of_steps" gilt in [1, RAD_PATH_MAX_STEPS]. Null Schritte sind kein
/// Pfad, sondern die Abwesenheit einer Bewegung -- wer nichts tut, schickt kein
/// Kommando und veroeffentlicht kein Ereignis. Die Plaetze hinter dem Zaehler
/// sind genullt und bedeuten nichts; wer einen Pfad bekommt, darf sich auf sie
/// nicht berufen.
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
