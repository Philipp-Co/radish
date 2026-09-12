#include "radish/game/game.h"
#include "radish/game/model/entity/entity.h"
#include "radish/game/model/tile/tile.h"
#include <radish/io/user_input.h>

#include <assert.h>
#include <stddef.h>
#include <stdio.h>


static void RAD_IoUserinputDefaultOnMoveActionStarted(const RAD_IoUserinputOnMoveActionStartedData_t *data);
static void RAD_IoUserinputDefaultOnMoveActionWaypointAdded(const RAD_IoUserinputOnMoveActionWaypointData_t *data);
static void RAD_IoUserinputDefaultOnMoveActionWaypointRejected(const RAD_IoUserinputOnMoveActionWaypointData_t *data);
static void RAD_IoUserinputDefaultOnMoveActionAccepted(const RAD_IoUserinputOnMoveActionAcceptedData_t *data);
static void RAD_IoUserinputDefaultOnMoveActionRequested(const RAD_IoUserinputOnMoveActionRequestedData_t *data);
static void RAD_IoUserinputDefaultOnMoveActionResponseReceived(const RAD_IoUserinputOnMoveActionResponseReceivedData_t *data);
static void RAD_IoUserinputDefaultOnMoveActionFinished(const RAD_IoUserinputOnMoveActionFinishedData_t *data);


void RAD_IoUserinputSubscribeToMoveActionEvents(
    RAD_IoUserInput_t *state,
    void *user_data,
    RAD_IoUserinputMoveActionCallbacks_t callbacks
)
{
    assert(NULL != state);
    state->move_action_subscriber.callback = callbacks;
    state->move_action_subscriber.user_argument = user_data;
}

RAD_IoUserInput_t RAD_CreateIoUserInputState(RAD_Game_t *game, RAD_IoUserinputSendCommandCallback_t send_callback)
{
    RAD_IoUserInput_t state = {
        .state = RAD_IO_USERINPUT_STATE_IDLE,
        .game = game,
        .send_command = send_callback,
        .move_action_subscriber = {
            .user_argument=NULL,
            .callback={
                .started=RAD_IoUserinputDefaultOnMoveActionStarted,
                .waypoint_added=RAD_IoUserinputDefaultOnMoveActionWaypointAdded,
                .waypoint_rejected=RAD_IoUserinputDefaultOnMoveActionWaypointRejected,
                .accepted=RAD_IoUserinputDefaultOnMoveActionAccepted,
                .action_requested=RAD_IoUserinputDefaultOnMoveActionRequested,
                .response_received=RAD_IoUserinputDefaultOnMoveActionResponseReceived,
                .finished=RAD_IoUserinputDefaultOnMoveActionFinished
            }
        }
    };
    return state;
}

void RAD_DestroyIoUserInput(RAD_IoUserInputState_t *state)
{
    (void)state;
}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleIdleOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);
static RAD_IoUserInputState_t RAD_IoUserinputStateHandleEntitySelectedOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);
static RAD_IoUserInputState_t RAD_IoUserinputStateHandleMoveOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);
static RAD_IoUserInputState_t RAD_IoUserinputStateHandleShooteOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);

static void RAD_IoUserinputStateToString(RAD_IoUserInputState_t state, char *output, int32_t len)
{
    switch(state)
    {
        case RAD_IO_USERINPUT_STATE_IDLE:
            (void)snprintf(output, len, "IDLE");
            break;
        case RAD_IO_USERINPUT_STATE_MOVE:
            (void)snprintf(output, len, "MOVE");
            break;
        case RAD_IO_USERINPUT_STATE_USE:
            (void)snprintf(output, len, "USE");
            break;
        case RAD_IO_USERINPUT_STATE_SHOOT:
            (void)snprintf(output, len, "SHOOT");
            break;
        case RAD_IO_USERINPUT_STATE_WAIT_FOR_ACK:
            (void)snprintf(output, len, "WAIT_FOR_ACK");
            break;
        default:
            break;
    }
}

void RAD_IoUserInputOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    char buffer[32];
    RAD_IoUserinputStateToString(state->state, buffer, sizeof(buffer));
    printf("User-Input-State %s\n", buffer);
    switch(state->state)
    {
        case RAD_IO_USERINPUT_STATE_IDLE:
            state->state = RAD_IoUserinputStateHandleIdleOnLeftClick(state, x, y);
            break;
        case RAD_IO_USERINPUT_STATE_ENTITY_SELECTED:
            state->state = RAD_IoUserinputStateHandleEntitySelectedOnLeftClick(state, x, y);
            break;
        case RAD_IO_USERINPUT_STATE_MOVE:
            state->state = RAD_IoUserinputStateHandleMoveOnLeftClick(state, x, y);
            break;
        case RAD_IO_USERINPUT_STATE_SHOOT:
            state->state = RAD_IoUserinputStateHandleShooteOnLeftClick(state, x, y);
        default:
           break; 
    }

    RAD_IoUserinputStateToString(state->state, buffer, sizeof(buffer));
    printf("    next State %s\n", buffer);
}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleIdleOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    //
    // idle -> idle: Selected Tile does not contain an Entity
    // idle -> entity_selected: Selected Tile contains an Entity
    //
    const bool result = RAD_GameTileAt(state->game, x, y, &state->selected_tile);
    if(!result)
    {
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    if(state->selected_tile.entity != RAD_ENTITY_NONE)
    {
        return RAD_IO_USERINPUT_STATE_ENTITY_SELECTED;
    }
    return RAD_IO_USERINPUT_STATE_IDLE;
}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleEntitySelectedOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    RAD_Tile_t tile;
    const bool result = RAD_GameTileAt(state->game, x, y, &tile);
    if(!result)
    {
        return RAD_IO_USERINPUT_STATE_IDLE;
    }

    const int16_t selected_x = state->selected_tile.x;
    const int16_t selected_y = state->selected_tile.y;

    if((tile.x == selected_x) && (tile.y == selected_y))
    {
        return RAD_IO_USERINPUT_STATE_SHOOT;
    }
    //
    // we are sure, that x, y point to a different tile then the selected one.
    //
    if(RAD_ENTITY_NONE == tile.entity)
    {
        RAD_IoUserinputOnMoveActionStartedData_t data = {
            .entity=NULL,
            .user_data=state->move_action_subscriber.user_argument,
            .x=-1,
            .y=-1
        };
        state->data.move.path.number_of_steps = 0;
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].x = selected_x;
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].y = selected_y;
        state->data.move.path.number_of_steps++;

        RAD_Entity_t entity;
        RAD_GameEntityAt(state->game, state->selected_tile.entity, &entity);
        data.entity = &entity;
        data.x = selected_x;
        data.y = selected_y;
        state->move_action_subscriber.callback.started(&data);
        
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].x = x;
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].y = y;
        state->data.move.path.number_of_steps++;
        RAD_IoUserinputOnMoveActionWaypointData_t waypoint_data = {
            .user_data=state->move_action_subscriber.user_argument,
            .x=tile.x,
            .y=tile.y
        };
        state->move_action_subscriber.callback.waypoint_added(&waypoint_data);

        return RAD_IO_USERINPUT_STATE_MOVE;
    }
    return RAD_IO_USERINPUT_STATE_ENTITY_SELECTED;
}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleMoveOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    RAD_Tile_t tile;
    const bool result = RAD_GameTileAt(state->game, x, y, &tile);
    if(!result)
    {
        RAD_IoUserinputOnMoveActionFinishedData_t data = {
            .user_data=state->move_action_subscriber.user_argument
        };
        state->move_action_subscriber.callback.finished(&data);
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    
    const int16_t selected_x = state->selected_tile.x;
    const int16_t selected_y = state->selected_tile.y;
    
    if((tile.x == selected_x) && (tile.y == selected_y))
    {
        state->data.move.path.number_of_steps = 0;
        RAD_IoUserinputOnMoveActionFinishedData_t data = {
            .user_data=state->move_action_subscriber.user_argument
        };
        state->move_action_subscriber.callback.finished(&data);
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    
    assert(state->data.move.path.number_of_steps >= 1);

    //
    // if clicked twice on the same Tile, send move Command.
    //
    if(
        (state->data.move.path.steps_to[state->data.move.path.number_of_steps-1].x == x) && 
        (state->data.move.path.steps_to[state->data.move.path.number_of_steps-1].y == y) 
    )
    {
        RAD_Command_t command;
        const RAD_EntityId_t entity_id = state->selected_tile.entity;

        RAD_IoUserinputOnMoveActionAcceptedData_t accepted_data = {
            .user_data=state->move_action_subscriber.user_argument
        };
        state->move_action_subscriber.callback.accepted(&accepted_data);

        if(RAD_GameMoveEntity(state->game, entity_id, &state->data.move.path, &command))
        {
            state->command_info.sequence = command.header.sequence;
            state->command_info.type = command.header.type;

            state->send_command(&command);

            RAD_IoUserinputOnMoveActionRequestedData_t requested_data = {
                .user_data=state->move_action_subscriber.user_argument
            };
            state->move_action_subscriber.callback.action_requested(&requested_data);
            return RAD_IO_USERINPUT_STATE_WAIT_FOR_ACK; 
        }
        //
        // If unable to create a Command -> invalid Move.
        //
        state->data.move.path.number_of_steps = 0;
        RAD_IoUserinputOnMoveActionFinishedData_t data = {
            .user_data=state->move_action_subscriber.user_argument
        };
        state->move_action_subscriber.callback.finished(&data);
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    else if(state->data.move.path.number_of_steps < RAD_PATH_MAX_STEPS)
    {
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].x = tile.x;
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].y = tile.y;
        state->data.move.path.number_of_steps++;
        RAD_IoUserinputOnMoveActionWaypointData_t data = {
            .user_data=state->move_action_subscriber.user_argument,
            .x=tile.x,
            .y=tile.y
        };
        state->move_action_subscriber.callback.waypoint_added(&data);
        return RAD_IO_USERINPUT_STATE_MOVE; 
    }
    //
    // Invalid Move, to many Steps.
    //
    state->data.move.path.number_of_steps = 0;
    RAD_IoUserinputOnMoveActionFinishedData_t data = {
        .user_data=state->move_action_subscriber.user_argument
    };
    state->move_action_subscriber.callback.finished(&data);
    return RAD_IO_USERINPUT_STATE_IDLE;
}

void RAD_IoUserInputOnCommandResponseReceived(RAD_IoUserInput_t *state, RAD_CommandResponse_t *response)
{
    switch(state->state)
    {
        case RAD_IO_USERINPUT_STATE_WAIT_FOR_ACK:
            if(response->header.sequence == state->command_info.sequence && response->header.type == state->command_info.type)
            {
                printf("Received Response to known command!\n");
            }
            RAD_IoUserinputOnMoveActionResponseReceivedData_t response_data = {
                .request_accepted=true,
                .user_data=state->move_action_subscriber.user_argument
            };
            state->move_action_subscriber.callback.response_received(&response_data);
            RAD_IoUserinputOnMoveActionFinishedData_t finished_data = {
                .user_data=state->move_action_subscriber.user_argument
            };
            state->move_action_subscriber.callback.finished(&finished_data);
            RAD_GameExecuteCommand(state->game, &response->command);
            state->state = RAD_IO_USERINPUT_STATE_IDLE;
            break;
        default:
            break;
    }
}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleShooteOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    RAD_Tile_t tile;
    const bool result = RAD_GameTileAt(state->game, x, y, &tile);
    if(!result)
    {
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    
    const int16_t selected_x = state->selected_tile.x;
    const int16_t selected_y = state->selected_tile.y;
    
    if((tile.x == selected_x) && (tile.y == selected_y))
    {
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    
    RAD_Command_t command;
    if(RAD_GameShoot(state->game, state->selected_tile.entity, x, y, &command))
    {
        state->send_command(&command);
        return RAD_IO_USERINPUT_STATE_WAIT_FOR_ACK; 
    }
    return RAD_IO_USERINPUT_STATE_IDLE;
}


static void RAD_IoUserinputDefaultOnMoveActionStarted(const RAD_IoUserinputOnMoveActionStartedData_t *data)
{

    (void)data;
}

static void RAD_IoUserinputDefaultOnMoveActionWaypointAdded(const RAD_IoUserinputOnMoveActionWaypointData_t *data)
{

    (void)data;
}

static void RAD_IoUserinputDefaultOnMoveActionWaypointRejected(const RAD_IoUserinputOnMoveActionWaypointData_t *data)
{

    (void)data;
}

static void RAD_IoUserinputDefaultOnMoveActionAccepted(const RAD_IoUserinputOnMoveActionAcceptedData_t *data)
{

    (void)data;
}

static void RAD_IoUserinputDefaultOnMoveActionRequested(const RAD_IoUserinputOnMoveActionRequestedData_t *data)
{
    (void)data;
}

static void RAD_IoUserinputDefaultOnMoveActionResponseReceived(const RAD_IoUserinputOnMoveActionResponseReceivedData_t *data)
{
    (void)data;
}

static void RAD_IoUserinputDefaultOnMoveActionFinished(const RAD_IoUserinputOnMoveActionFinishedData_t *data)
{
    (void)data;
}
