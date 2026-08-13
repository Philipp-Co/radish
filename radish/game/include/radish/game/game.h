#ifndef __RAD_GAME_H__
#define __RAD_GAME_H__

#include <radish/game/world.h>
#include <radish/game/events/event_manager.h>

///
/// Oberste Abstraktion. Ein Spiel besteht aus genau einer Welt; sie liegt per
/// Wert im Spiel, damit die Ownership eindeutig ist und es nur eine Allokation
/// gibt.
///
typedef struct
{
    RAD_World_t world;

    RAD_EventManager_t *event_manager;
} RAD_Game_t;


RAD_Game_t* RAD_CreateGame(RAD_EventManager_t *event_manager);
void RAD_DestroyGame(RAD_Game_t **game);


#endif
