#ifndef __RAD_IO_USER_INPUT_H__
#define __RAD_IO_USER_INPUT_H__

#include "radish/game/model/entity/entity.h"
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
    RAD_IoUserInputState_t old_state; 
    RAD_IoUserInputState_t new_state; 
} RAD_IoUserinputStateChanged_t;

typedef struct
{
    void *user_data;    
    struct {
        int16_t x;
        int16_t y;
    } coordinates;
} RAD_IoUserinputSelectedEntityChanged_t;


typedef struct
{
    void *user_data;    
    const RAD_Entity_t *entity;
    int16_t x;
    int16_t y;
} RAD_IoUserinputOnMoveActionStartedData_t;
typedef void (*RAD_IoUserinputOnMoveActionStarted_t)(const RAD_IoUserinputOnMoveActionStartedData_t *data);

typedef struct 
{
    void *user_data;    
    int16_t x;
    int16_t y;
} RAD_IoUserinputOnMoveActionWaypointData_t;
typedef void (*RAD_IoUserinputOnMoveActionWaypointAdded_t)(const RAD_IoUserinputOnMoveActionWaypointData_t *data);
typedef void (*RAD_IoUserinputOnMoveActionWaypointRejected_t)(const RAD_IoUserinputOnMoveActionWaypointData_t *data);

typedef struct
{
    void *user_data;    
} RAD_IoUserinputOnMoveActionAcceptedData_t; 
typedef void (*RAD_IoUserinputOnMoveActionAccepted_t)(const RAD_IoUserinputOnMoveActionAcceptedData_t *data);

typedef struct
{
    void *user_data;    
} RAD_IoUserinputOnMoveActionRequestedData_t; 
typedef void (*RAD_IoUserinputOnMoveActionRequested_t)(const RAD_IoUserinputOnMoveActionRequestedData_t *data);

typedef struct 
{
    void *user_data;    
    bool request_accepted;
} RAD_IoUserinputOnMoveActionResponseReceivedData_t;
typedef void (*RAD_IoUserinputOnMoveActionResponseReceived_t)(const RAD_IoUserinputOnMoveActionResponseReceivedData_t *data);

typedef struct
{
    void *user_data;
} RAD_IoUserinputOnMoveActionFinishedData_t;
typedef void (*RAD_IoUserinputOnMoveActionFinished_t)(const RAD_IoUserinputOnMoveActionFinishedData_t *data);

typedef struct
{
    ///
    /// \brief  User started a Move-Action.
    ///
    RAD_IoUserinputOnMoveActionStarted_t started;
    ///
    /// \brief  User accepted the compiled Waypoints.
    ///
    RAD_IoUserinputOnMoveActionAccepted_t accepted;
    ///
    /// \brief  A new Waypoint was added to the Path.
    ///
    RAD_IoUserinputOnMoveActionWaypointAdded_t waypoint_added;
    ///
    /// \brief  A requested Waypoint was rejected.
    ///
    RAD_IoUserinputOnMoveActionWaypointRejected_t waypoint_rejected;
    ///
    /// \brief  The Client send a Request to the Server.
    /// 
    RAD_IoUserinputOnMoveActionRequested_t action_requested;
    ///
    /// \brief  Received a Response from the Server.
    ///
    RAD_IoUserinputOnMoveActionResponseReceived_t response_received;
    RAD_IoUserinputOnMoveActionFinished_t finished;
} RAD_IoUserinputMoveActionCallbacks_t;

typedef void (*RAD_IoUserinputOnShootActionStarted_t)();
typedef void (*RAD_IoUserinputOnShootActionDirectionSpecified_t)();
typedef void (*RAD_IoUserinputOnShootActionAccepted_t)();
typedef void (*RAD_IoUserinputOnShootActionRequested_t)();
typedef void (*RAD_IoUserinputOnShootActionResponseReceived_t)();


typedef bool (*RAD_IoUserinputSendCommandCallback_t)(const RAD_Command_t *command);


typedef struct
{
    RAD_EntityPath_t path;
} RAD_IoUserinputStateMove_t;

typedef struct
{
    union
    {
        RAD_IoUserinputStateMove_t move;
    } data;
    
    RAD_Tile_t selected_tile;

    RAD_IoUserInputState_t state; 
    RAD_Game_t *game;

    RAD_IoUserinputSendCommandCallback_t send_command;
    struct 
    {
        uint32_t sequence;
        RAD_CommandType_t type;
    } command_info;

    struct
    {
        RAD_IoUserinputMoveActionCallbacks_t callback;
        void *user_argument;
    } move_action_subscriber;
} RAD_IoUserInput_t;

void RAD_IoUserinputSubscribeToMoveActionEvents(
    RAD_IoUserInput_t *state,
    void *user_data,
    RAD_IoUserinputMoveActionCallbacks_t callbacks
);

RAD_IoUserInput_t RAD_CreateIoUserInputState(RAD_Game_t *game, RAD_IoUserinputSendCommandCallback_t send_callback);
void RAD_DestroyIoUserInput(RAD_IoUserInputState_t *state);

void RAD_IoUserInputOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);
void RAD_IoUserInputOnRightClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);
void RAD_IoUserInputOnCommandResponseReceived(RAD_IoUserInput_t *state, RAD_CommandResponse_t *response);

#endif
