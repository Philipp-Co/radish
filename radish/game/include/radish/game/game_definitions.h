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


#endif
