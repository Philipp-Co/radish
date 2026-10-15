#ifndef __RAD_GAME_DEFINITIONS_H__
#define __RAD_GAME_DEFINITIONS_H__

///
/// Die groesste Welt, die das Programm halten kann. Wie gross eine Welt wirklich
/// ist, steht in ihr selbst (RAD_World_t.width/height) und kommt aus der
/// Weltdefinition; diese zwei Zahlen bemessen nur den Speicher dafuer, und der
/// Loader weist eine groessere Welt ab.
///
#define RAD_WORLD_WIDTH  16
#define RAD_WORLD_HEIGHT 16

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
/// werden, aber viel frueher ist Schluss: acht Spieler.
///
/// Sie steht hier neben der Groesse der Welt und nicht beim Server, seit das
/// Spiel selbst weiss, wer mitspielt (turn.h) -- wie viele das sein duerfen,
/// ist damit eine Festlegung der Regeln und keine des Programms, das sie haelt.
///
#define RAD_MAX_PLAYERS 8

///
/// Wie viele mitspielen duerfen: genau zwei, einer je Kante (RAD_PlayerEdge_t,
/// model/player/player.h). Eine Regel des Spiels, enger als RAD_MAX_PLAYERS --
/// die Reihe des Zuges (turn.h) bleibt allgemein, die Grenze zieht
/// RAD_GameAddPlayer.
///
#define RAD_GAME_NUMBER_OF_PLAYERS 2

///
/// Wie tief die Aufstellzone eines Spielers von seiner Kante ins Feld reicht, in
/// Spalten: die Kante selbst mitgezaehlt (RAD_GameDeployZone).
///
#define RAD_DEPLOY_ZONE_DEPTH 4

///
/// Nach wie vielen Runden gewertet wird (RAD_GameScore): ist die letzte davon
/// um, ist das Spiel vorbei.
///
#define RAD_GAME_NUMBER_OF_ROUNDS 10

///
/// Punkte je Siegfeld (RAD_GameScore): eine punktende Einheit auf dem Feld, und
/// je eine auf einem der vier Nachbarfelder.
///
#define RAD_VICTORY_TILE_POINTS 20
#define RAD_VICTORY_NEIGHBOUR_POINTS 5


#endif
