#include "radish/rendering/entity.h"
#include <radish/rendering/game_events.h>
#include <radish/rendering/iso_map.h>
#include <radish/math.h>

#include <stdio.h>

static RAD_EntityPath_t path;

void RAD_RenderingOnMoveActionStarted(const RAD_IoUserinputOnMoveActionStartedData_t *data)
{
    //
    // The User started a Move Command.
    //
    RAD_IsoMap_t *map = (RAD_IsoMap_t*)data->user_data;
    
    const int16_t x = data->entity->x;
    const int16_t y = data->entity->y;
    const int16_t z = 0;

    RAD_IsoEntity_t *iso_entity = map->data[z][y][x].entity;
    if(NULL == iso_entity)
    {
        printf("Move started on a Tile with no Entity!\n");
        return;
    }
    if(iso_entity->id != data->entity->id)
    {
        printf("Error, given Entity-Id does not fit...\n");
        return;
    }
    printf("Start Move-Action for Entity: %i\n", data->entity->id);

    path.number_of_steps = 1;
    path.steps_to[0].x = x;
    path.steps_to[0].y = y;
    
    map->data[z][y][x].background_color.b = 255;
    map->data[z][y][x].background_color.r = 0;
    map->data[z][y][x].background_color.g = 0;
}

void RAD_RenderingOnMoveActionWaypointAdded(const RAD_IoUserinputOnMoveActionWaypointData_t *data)
{
    //
    // A Waypoint was added by the User.
    //
    printf("Waypoint added %i, %i\n", data->x, data->y);
    RAD_IsoMap_t *map = (RAD_IsoMap_t*)data->user_data;

    const int16_t x = data->x;
    const int16_t y = data->y;
    const int8_t z = 0;

    if(x == path.steps_to[path.number_of_steps-1].x)
    {
        //
        // y changes.
        //
        const int32_t delta_y = y - path.steps_to[path.number_of_steps-1].y;
        for(int32_t i=delta_y < 0 ? delta_y : 0;i<(delta_y < 0 ? 0 : delta_y);++i)
        {
            map->data[z][y-i][x].background_color.b = 255;
            map->data[z][y-i][x].background_color.r = 0;
            map->data[z][y-i][x].background_color.g = 0;
        }
    }
    else
    {
        const int32_t delta_x = x - path.steps_to[path.number_of_steps-1].x;
        for(int32_t i=delta_x < 0 ? delta_x : 0;i<(delta_x < 0 ? 0 : delta_x);++i)
        {
            map->data[z][y][x-i].background_color.b = 255;
            map->data[z][y][x-i].background_color.r = 0;
            map->data[z][y][x-i].background_color.g = 0;
        }
    }
    map->data[z][y][x].background_color.b = 255;
    map->data[z][y][x].background_color.r = 0;
    map->data[z][y][x].background_color.g = 0;

    path.steps_to[path.number_of_steps].x = x;
    path.steps_to[path.number_of_steps].y = y;
    path.number_of_steps++;
}

void RAD_RenderingOnMoveActionWaypointRejected(const RAD_IoUserinputOnMoveActionWaypointData_t *data)
{
    //
    // The User tried to add a Waypoint, the Control-Layer rejected it.
    // -> Inform User
    //
    printf("Waypoint %i, %i rejected\n", data->x, data->y);
}

void RAD_RenderingOnMoveActionAccepted(const RAD_IoUserinputOnMoveActionAcceptedData_t *data)
{
    //
    // User has accepted his Move Command.
    // -> The Path if fully given.
    // -> No more Userinput allowed.
    //
    printf("Action accepted\n");
}

void RAD_RenderingOnMoveActionRequested(const RAD_IoUserinputOnMoveActionRequestedData_t *data)
{
    //
    // The Application has issued a Move Command to the Server.
    // -> Now the Client is waiting for its Response.
    //
    printf("Action Requested\n");
}

void RAD_RenderingOnMoveActionResponseReceived(const RAD_IoUserinputOnMoveActionResponseReceivedData_t *data)
{
    //
    // The Server has responded to the Move Command.
    // -> Evaluate and display the Result.
    //
    printf("Response Received!!\n"); 
}

void RAD_RenderingOnMoveActionFinished(const RAD_IoUserinputOnMoveActionFinishedData_t *data)
{
    //
    // The Move Command has Finished.
    // -> Repaint everything in conjunction with the Move Command.
    //
    printf("Move Action finished!\n");
    RAD_IsoMap_t *map = (RAD_IsoMap_t*)data->user_data;

    for(int32_t i=0;i<path.number_of_steps-1;++i)
    {
        const int16_t x_0 = path.steps_to[i].x;
        const int16_t y_0 = path.steps_to[i].y;
        const int16_t x_1 = path.steps_to[i+1].x;
        const int16_t y_1 = path.steps_to[i+1].y;
        const int16_t z = 0;

        if(x_0 == x_1)
        {
            for(int32_t i=min(y_0, y_1);i<=max(y_0, y_1);++i)
            {
                map->data[z][i][x_0].background_color.b = 255;
                map->data[z][i][x_0].background_color.r = 255;
                map->data[z][i][x_0].background_color.g = 255;
            }
        }
        else
        {
            for(int32_t i=min(x_0, x_1);i<=max(x_0, x_1);++i)
            {
                map->data[z][y_0][i].background_color.b = 255;
                map->data[z][y_0][i].background_color.r = 255;
                map->data[z][y_0][i].background_color.g = 255;
            }
        }
    }
    const int16_t x = path.steps_to[path.number_of_steps-1].x;
    const int16_t y = path.steps_to[path.number_of_steps-1].y;
    const int16_t z = 0;
    map->data[z][y][x].background_color.b = 255;
    map->data[z][y][x].background_color.r = 255;
    map->data[z][y][x].background_color.g = 255;
}
