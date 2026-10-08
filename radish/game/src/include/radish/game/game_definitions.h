#ifndef __RAD_GAME_DEFINITIONS_H__
#define __RAD_GAME_DEFINITIONS_H__

///
/// Die groesste Welt, die das Programm halten kann. Wie gross eine Welt wirklich
/// ist, steht in ihr selbst (RAD_World_t.width/height) und kommt aus der
/// Weltdefinition; diese zwei Zahlen bemessen nur den Speicher dafuer, und der
/// Loader weist eine groessere Welt ab.
///
#define RAD_WORLD_WIDTH  8
#define RAD_WORLD_HEIGHT 8

///
/// Plaetze im Einheitenpool, ueber das ganze Spiel. Gezaehlt wird jede Einheit, die
/// das Spiel kennt -- die Armeen in der Reserve, was auf dem Feld steht, und was
/// zerstoert wurde, denn eine Id wird nie neu vergeben (unit.h).
///
/// Die Zahl haengt deshalb nicht mehr an der Groesse der Welt. Auf dem Feld kann
/// es nie mehr Einheiten geben als Tiles, aber die Reserve steht auf keinem. 64
/// reicht fuer zwei Armeen von je gut dreissig Einheiten; ist der Pool voll, lehnt
/// die Welt jede weitere ab (RAD_WorldAddReserveUnit, RAD_WorldSpawnUnit liefern
/// RAD_UNIT_NONE).
///
#define RAD_MAX_UNITS 64

///
/// Obergrenze der Mitspieler. Mehr als RAD_MAX_UNITS koennten es ohnehin nie
/// werden, aber viel frueher ist Schluss: acht Spieler auf 8x8 Tiles.
///
/// Sie steht hier neben der Groesse der Welt und nicht beim Server, seit das
/// Spiel selbst weiss, wer mitspielt (turn.h) -- wie viele das sein duerfen,
/// ist damit eine Festlegung der Regeln und keine des Programms, das sie haelt.
///
#define RAD_MAX_PLAYERS 8


#endif
