#include <radish/view/user_input.h>

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

RAD_IoUserInput_t RAD_CreateIoUserInputState(const RAD_ClientWorld_t *world, RAD_NetUserId_t user, RAD_IoUserinputSendCommandCallback_t send_callback)
{
    RAD_IoUserInput_t state = {
        .state = RAD_IO_USERINPUT_STATE_IDLE,
        .world = world,
        .user = user,
        .next_sequence = 1,
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
            break;
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
    const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt(state->world, x, y);
    if(tile == NULL)
    {
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    state->selected_tile = tile;
    if(RAD_ClientTileHasUnit(state->selected_tile))
    {
        return RAD_IO_USERINPUT_STATE_ENTITY_SELECTED;
    }
    return RAD_IO_USERINPUT_STATE_IDLE;
}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleEntitySelectedOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt(state->world, x, y);
    if(tile == NULL)
    {
        return RAD_IO_USERINPUT_STATE_IDLE;
    }

    const int16_t selected_x = (int16_t)state->selected_tile->x;
    const int16_t selected_y = (int16_t)state->selected_tile->y;

    if(((int16_t)tile->x == selected_x) && ((int16_t)tile->y == selected_y))
    {
        return RAD_IO_USERINPUT_STATE_SHOOT;
    }
    //
    // we are sure, that x, y point to a different tile then the selected one.
    //
    if(!RAD_ClientTileHasUnit(tile))
    {
        RAD_IoUserinputOnMoveActionStartedData_t data = {
            .entity_id=state->selected_tile->unit->id,
            .user_data=state->move_action_subscriber.user_argument,
            .x=selected_x,
            .y=selected_y
        };
        state->data.move.path.number_of_steps = 0;
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].x = selected_x;
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].y = selected_y;
        state->data.move.path.number_of_steps++;

        state->move_action_subscriber.callback.started(&data);
        
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].x = x;
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].y = y;
        state->data.move.path.number_of_steps++;
        RAD_IoUserinputOnMoveActionWaypointData_t waypoint_data = {
            .user_data=state->move_action_subscriber.user_argument,
            .x=(int16_t)tile->x,
            .y=(int16_t)tile->y
        };
        state->move_action_subscriber.callback.waypoint_added(&waypoint_data);

        return RAD_IO_USERINPUT_STATE_MOVE;
    }
    return RAD_IO_USERINPUT_STATE_ENTITY_SELECTED;
}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleMoveOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt(state->world, x, y);
    if(tile == NULL)
    {
        RAD_IoUserinputOnMoveActionFinishedData_t data = {
            .user_data=state->move_action_subscriber.user_argument
        };
        state->move_action_subscriber.callback.finished(&data);
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    
    const int16_t selected_x = (int16_t)state->selected_tile->x;
    const int16_t selected_y = (int16_t)state->selected_tile->y;
    
    if(((int16_t)tile->x == selected_x) && ((int16_t)tile->y == selected_y))
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
        RAD_IoUserinputOnMoveActionAcceptedData_t accepted_data = {
            .user_data=state->move_action_subscriber.user_argument
        };
        state->move_action_subscriber.callback.accepted(&accepted_data);

        //
        // A Path needs at least the start and one more Tile (net_types.h).
        // selected_tile points into the World: its Unit may have left since.
        //
        if((state->data.move.path.number_of_steps >= 2) && RAD_ClientTileHasUnit(state->selected_tile))
        {
            const RAD_NetMoveRequest_t request = {
                .sequence=state->next_sequence++,
                .user=state->user,
                .entity=state->selected_tile->unit->id,
                .path=state->data.move.path
            };
            state->command_info.sequence = request.sequence;

            if(state->send_command(&request))
            {
                RAD_IoUserinputOnMoveActionRequestedData_t requested_data = {
                    .user_data=state->move_action_subscriber.user_argument
                };
                state->move_action_subscriber.callback.action_requested(&requested_data);
                return RAD_IO_USERINPUT_STATE_WAIT_FOR_ACK;
            }
        }
        //
        // Invalid Move or not sent -> no Response will come.
        //
        state->data.move.path.number_of_steps = 0;
        RAD_IoUserinputOnMoveActionFinishedData_t data = {
            .user_data=state->move_action_subscriber.user_argument
        };
        state->move_action_subscriber.callback.finished(&data);
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    else if(state->data.move.path.number_of_steps < RAD_NET_PATH_MAX_STEPS)
    {
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].x = (int16_t)tile->x;
        state->data.move.path.steps_to[state->data.move.path.number_of_steps].y = (int16_t)tile->y;
        state->data.move.path.number_of_steps++;
        RAD_IoUserinputOnMoveActionWaypointData_t data = {
            .user_data=state->move_action_subscriber.user_argument,
            .x=(int16_t)tile->x,
            .y=(int16_t)tile->y
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

void RAD_IoUserInputOnCommandResponseReceived(RAD_IoUserInput_t *state, const RAD_NetCommandResponse_t *response)
{
    switch(state->state)
    {
        case RAD_IO_USERINPUT_STATE_WAIT_FOR_ACK:
            if(response->sequence == state->command_info.sequence)
            {
                printf("Received Response to known command!\n");
            }
            RAD_IoUserinputOnMoveActionResponseReceivedData_t response_data = {
                .request_accepted=response->success,
                .user_data=state->move_action_subscriber.user_argument
            };
            state->move_action_subscriber.callback.response_received(&response_data);
            RAD_IoUserinputOnMoveActionFinishedData_t finished_data = {
                .user_data=state->move_action_subscriber.user_argument
            };
            state->move_action_subscriber.callback.finished(&finished_data);
            state->state = RAD_IO_USERINPUT_STATE_IDLE;
            break;
        default:
            break;
    }
}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleShooteOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt(state->world, x, y);
    if(tile == NULL)
    {
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    
    const int16_t selected_x = (int16_t)state->selected_tile->x;
    const int16_t selected_y = (int16_t)state->selected_tile->y;
    
    if(((int16_t)tile->x == selected_x) && ((int16_t)tile->y == selected_y))
    {
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    
    //
    // Shoot is not sent yet: the protocol targets an Entity, the Client selects
    // a Tile (net_codec.h).
    //
    printf("Shoot noch nicht unterstuetzt\n");
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
