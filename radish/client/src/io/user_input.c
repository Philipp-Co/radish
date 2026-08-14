#include <radish/io/user_input.h>

#include <stddef.h>
#include <stdio.h>


RAD_IoUserInput_t RAD_CreateIoUserInputState(RAD_Game_t *game, RAD_IoUserinputSendCommandCallback_t send_callback)
{
    RAD_IoUserInput_t state = {
        .state = RAD_IO_USERINPUT_STATE_IDLE,
        .game = game,
        .send_command = send_callback
    };
    return state;
}

void RAD_DestroyIoUserInput(RAD_IoUserInputState_t *state)
{

}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleIdleOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);
static RAD_IoUserInputState_t RAD_IoUserinputStateHandleEntitySelectedOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);
static RAD_IoUserInputState_t RAD_IoUserinputStateHandleMoveOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);
static RAD_IoUserInputState_t RAD_IoUserinputStateHandleMoveOnRightClick(RAD_IoUserInput_t *state, int32_t x, int32_t y);

void RAD_IoUserInputOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    printf("State %i\n", state->state);
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
        default:
           break; 
    }
    printf("    next State %i\n", state->state);
}

void RAD_IoUserInputOnRightClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{

}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleIdleOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    const RAD_Tile_t *tile = RAD_WorldTileAt(&state->game->world, x, y);
    if(NULL == tile || (tile != NULL && RAD_ENTITY_NONE == tile->entity))
    {
        printf("No tile found...\n");
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    printf("Selected tile %i, %i, entity: %i\n", x, y, tile->entity);
    state->selected_tile = tile;
    if(tile->entity != RAD_ENTITY_NONE)
    {
        printf("Selected Entity %i\n", tile->entity);
    }
    return RAD_IO_USERINPUT_STATE_ENTITY_SELECTED;
}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleEntitySelectedOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    const RAD_Tile_t *tile = RAD_WorldTileAt(&state->game->world, x, y);
    if(tile == state->selected_tile)
    {
        return RAD_IO_USERINPUT_STATE_IDLE;
    }
    const RAD_Entity_t *entity = RAD_WorldEntityAt(&state->game->world, x, y);
    if((NULL != tile) && (NULL == entity))
    {
        printf("Move to...%i, %i\n", x, y);
        state->data.move.target[state->data.move.number_of_waypoints++] = tile;
        return RAD_IO_USERINPUT_STATE_MOVE;
    }
    if(NULL != tile)
        printf("Clicked on %i, %i (%p, %i); Nothing happens...\n", x, y, tile, tile->entity);
    return RAD_IO_USERINPUT_STATE_ENTITY_SELECTED;
}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleMoveOnLeftClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    printf("Send Move command!\n"); 
    RAD_Command_t command;
    const RAD_EntityId_t entity_id = state->selected_tile->entity;

    RAD_GameMoveEntity(state->game, entity_id, state->selected_tile->x, state->selected_tile->y, x, y, &command);
    state->command_info.sequence = command.header.sequence;
    state->command_info.type = command.header.type;
    state->send_command(&command);
    return RAD_IO_USERINPUT_STATE_WAIT_FOR_ACK; 
}

static RAD_IoUserInputState_t RAD_IoUserinputStateHandleMoveOnRightClick(RAD_IoUserInput_t *state, int32_t x, int32_t y)
{
    printf("Reset state...\n");
    return RAD_IO_USERINPUT_STATE_ENTITY_SELECTED; 
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
            state->selected_tile = NULL;
            state->state = RAD_IO_USERINPUT_STATE_IDLE;
            break;
        default:
            break;
    }
}

