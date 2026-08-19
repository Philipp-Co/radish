#ifndef __RAD_GAME_DEFINITIONS_H__
#define __RAD_GAME_DEFINITIONS_H__


#define RAD_WORLD_WIDTH  8
#define RAD_WORLD_HEIGHT 8

///
/// Da pro Tile hoechstens eine Entitaet stehen darf, kann es nie mehr als
/// RAD_WORLD_WIDTH * RAD_WORLD_HEIGHT gleichzeitig geben -- der Pool ist damit
/// exakt gross genug und RAD_WorldSpawnEntity kann nie an einem vollen Pool
/// scheitern, solange die Belegungspruefung vorher greift.
///
#define RAD_MAX_ENTITIES (RAD_WORLD_WIDTH * RAD_WORLD_HEIGHT)

///
/// Obergrenze der Mitspieler. Mehr als RAD_MAX_ENTITIES koennten es ohnehin nie
/// werden, aber viel frueher ist Schluss: acht Spieler auf 8x8 Tiles.
///
/// Sie steht hier neben der Groesse der Welt und nicht beim Server, seit das
/// Spiel selbst weiss, wer mitspielt (turn.h) -- wie viele das sein duerfen,
/// ist damit eine Festlegung der Regeln und keine des Programms, das sie haelt.
///
#define RAD_MAX_PLAYERS 8

///
/// Was ein Mitspieler in einer Runde tun kann, gezaehlt in Aktionspunkten. Jeder
/// hat zwei, jede Runde neu; was ein einzelnes Kommando davon kostet, steht nicht
/// hier, sondern bei der Regel, die es ausfuehrt.
///
/// Neben RAD_MAX_PLAYERS und der Groesse der Welt, weil es dieselbe Art
/// Festlegung ist: eine Zahl, die das Spiel ausmacht und an keinem Programm
/// haengt, das es haelt.
///
#define RAD_ACTION_POINTS_PER_TURN 2


#endif
