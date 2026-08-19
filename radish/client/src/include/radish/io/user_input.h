#ifndef __RAD_IO_USER_INPUT_H__
#define __RAD_IO_USER_INPUT_H__

#include <stdint.h>
#include <radish/game/game.h>
#include <radish/game/model/tile/tile.h>
#include <radish/game/control/command/command.h>

//
// Click in a tile:
//  Sets focus to that tile. Options are derivated from tile and entity on that tile.
//
//  If tile is empty
//      - show Info for that tile
//
//  If tile has an entity on it
//      - click on the same time resets the state
//      - each command has to be acknoleged by a left-click
//      - move, click on an empty tile tirggers that move
//      - shoot, right-click on any tile in the same row or column triggers that shoot-aktion
//      - use, click on a usable neigboring Entity triggers that use
//      
//
//  idle -> entity_selected
//  
//  entity_selected -> about_to_move
//  entity_selected -> about_to_shott
//  entity_selected -> about_to_use
//
//
//

typedef bool (*RAD_IoUserinputSendCommandCallback_t)(const RAD_Command_t *command);

typedef enum
{
    RAD_IO_USERINPUT_STATE_IDLE = 0,
    RAD_IO_USERINPUT_STATE_ENTITY_SELECTED,
    RAD_IO_USERINPUT_STATE_MOVE,
    RAD_IO_USERINPUT_STATE_SHOOT,
    RAD_IO_USERINPUT_STATE_USE,
    RAD_IO_USERINPUT_STATE_WAIT_FOR_ACK
} RAD_IoUserInputState_t;

typedef struct
{
    const RAD_Tile_t* target[8];
    int8_t number_of_waypoints;
} RAD_IoUserinputStateMove_t;

typedef struct
{
    union
    {
        RAD_IoUserinputStateMove_t move;
    } data;
    
    const RAD_Tile_t *selected_tile;

    RAD_IoUserInputState_t state; 
    RAD_Game_t *game;

    RAD_IoUserinputSendCommandCallback_t send_command;
    struct 
    {
        uint32_t sequence;
        RAD_CommandType_t type;
    } command_info;
} RAD_IoUserInput_t;

RAD_IoUserInput_t RAD_CreateIoUserInputState(RAD_Game_t *game, RAD_IoUserinputSendCommandCallback_t send_callback);
void RAD_DestroyIoUserInput(RAD_IoUserInputState_t *state);

void RAD_IoUserInputOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);
void RAD_IoUserInputOnRightClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);
void RAD_IoUserInputOnCommandResponseReceived(RAD_IoUserInput_t *state, RAD_CommandResponse_t *response);

#endif
