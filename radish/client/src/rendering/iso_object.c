#include "SDL2/SDL_render.h"
#include "radish/rendering/iso_definitions.h"
#include <radish/rendering/iso_object.h>

void RAD_ToIsoCoordinates(int32_t x, int32_t y, int32_t *screen_x, int32_t *screen_y)
{
    //*screen_x = MAP_RENDER_OFFSET_X + ((x * RAD_ISO_TILE_WIDTH / 2) + (y * RAD_ISO_TILE_WIDTH / 2));
    //*screen_y = MAP_RENDER_OFFSET_Y + ((y * RAD_ISO_TILE_HEIGHT / 2) - (x * RAD_ISO_TILE_HEIGHT / 2));
    *screen_x = ((x * RAD_ISO_TILE_WIDTH / 2) + (y * RAD_ISO_TILE_WIDTH / 2));
    *screen_y = ((y * RAD_ISO_TILE_HEIGHT / 2) - (x * RAD_ISO_TILE_HEIGHT / 2));
}

RAD_IsoObject_t RAD_CreateIsoObject(int32_t x, int32_t y, int32_t layer)
{
    int32_t screen_x = 0;
    int32_t screen_y = 0;
    RAD_ToIsoCoordinates(x, y, &screen_x, &screen_y);
    SDL_Color background_color = {
        255, 255, 255, 255
    };
    SDL_Color front_color = {
        255, 255, 255, 255
    };
    RAD_IsoObject_t object = {
        .x = x,
        .y = y,
        .screen_x = x + screen_x,
        .screen_y = y + screen_y,
        .background_color = background_color,
        .front_color = front_color,
        .focus = false,
        .layer = layer
    };
    return object;
}

void RAD_RenderIsoObject(SDL_Renderer *renderer, RAD_IsoObject_t *object, int32_t offset_x, int32_t offset_y, RAD_Camera_t *camera)
{
    SDL_Color color = object->background_color;
    if(object->focus)
    {
        color.r = 255;
        color.b = 0;
        color.g = 0;
        color.a = 255;
    }

    int32_t screen_x = object->screen_x - camera->x + offset_x;
    int32_t screen_y = object->screen_y - camera->y + object->layer * -RAD_ISO_TILE_HEIGHT + offset_y;

    SDL_SetRenderDrawColor(
        renderer, 
        color.r,
        color.g,
        color.b,
        color.a
    );
    SDL_Vertex vertecies[6];
    vertecies[0] = (SDL_Vertex){
        .position={
            .x=screen_x,
            .y=screen_y + RAD_ISO_TILE_HEIGHT / 2
        },
        .color=color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    vertecies[1] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH / 2,
            .y=screen_y
        },
        .color=color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    vertecies[2] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH / 2,
            .y=screen_y + RAD_ISO_TILE_HEIGHT
        },
        .color=color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    vertecies[3] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH / 2,
            .y=screen_y + RAD_ISO_TILE_HEIGHT
        },
        .color=color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    vertecies[4] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH / 2,
            .y=screen_y
        },
        .color=color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    vertecies[5] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH,
            .y=screen_y + RAD_ISO_TILE_HEIGHT / 2
        },
        .color=color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    SDL_RenderGeometry(
        renderer, NULL, vertecies, 3, NULL, 0 
    );
    SDL_RenderGeometry(
        renderer, NULL, vertecies + 3, 3, NULL, 0 
    );

    SDL_Vertex front_left_0[3];
    front_left_0[0] = (SDL_Vertex){
        .position={
            .x=screen_x,
            .y=screen_y + RAD_ISO_TILE_HEIGHT / 2
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    front_left_0[1] = (SDL_Vertex){
        .position={
            .x=screen_x,
            .y=screen_y + RAD_ISO_TILE_HEIGHT + RAD_ISO_TILE_HEIGHT / 2
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    front_left_0[2] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH / 2,
            .y=screen_y + RAD_ISO_TILE_HEIGHT * 2
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    SDL_RenderGeometry(
        renderer, NULL, front_left_0, 3, NULL, 0 
    );
    
    SDL_Vertex front_left_1[3];
    front_left_1[0] = (SDL_Vertex){
        .position={
            .x=screen_x,
            .y=screen_y + RAD_ISO_TILE_HEIGHT / 2
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    front_left_1[1] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH / 2,
            .y=screen_y + RAD_ISO_TILE_HEIGHT
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    front_left_1[2] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH / 2,
            .y=screen_y + RAD_ISO_TILE_HEIGHT * 2
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    SDL_RenderGeometry(
        renderer, NULL, front_left_1, 3, NULL, 0 
    );
    
    SDL_Vertex front_right_0[3];
    front_right_0[0] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH / 2,
            .y=screen_y + RAD_ISO_TILE_HEIGHT
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    front_right_0[1] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH / 2,
            .y=screen_y + RAD_ISO_TILE_HEIGHT * 2
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    front_right_0[2] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH,
            .y=screen_y + RAD_ISO_TILE_HEIGHT / 2
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    SDL_RenderGeometry(
        renderer, NULL, front_right_0, 3, NULL, 0 
    );
    SDL_Vertex front_right_1[3];
    front_right_1[0] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH,
            .y=screen_y + RAD_ISO_TILE_HEIGHT / 2
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    front_right_1[1] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH,
            .y=screen_y + RAD_ISO_TILE_HEIGHT + RAD_ISO_TILE_HEIGHT / 2
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    front_right_1[2] = (SDL_Vertex){
        .position={
            .x=screen_x + RAD_ISO_TILE_WIDTH / 2,
            .y=screen_y + RAD_ISO_TILE_HEIGHT * 2
        },
        .color=object->front_color,
        .tex_coord={.x=0.0, .y=0.0}
    };
    SDL_RenderGeometry(
        renderer, NULL, front_right_1, 3, NULL, 0 
    );

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, color.a);
    SDL_RenderDrawLine(
        renderer, 
        screen_x, screen_y + RAD_ISO_TILE_HEIGHT / 2, 
        screen_x + RAD_ISO_TILE_WIDTH / 2, screen_y
    );
    SDL_RenderDrawLine(
        renderer, 
        screen_x + RAD_ISO_TILE_WIDTH / 2, screen_y, 
        screen_x + RAD_ISO_TILE_WIDTH, screen_y + RAD_ISO_TILE_HEIGHT / 2
    );
    SDL_RenderDrawLine(
        renderer, 
        screen_x + RAD_ISO_TILE_WIDTH, screen_y + RAD_ISO_TILE_HEIGHT / 2, 
        screen_x + RAD_ISO_TILE_WIDTH, screen_y + RAD_ISO_TILE_HEIGHT / 2 + RAD_ISO_TILE_HEIGHT
    );
    SDL_RenderDrawLine(
        renderer, 
        screen_x, screen_y + RAD_ISO_TILE_HEIGHT / 2, 
        screen_x, screen_y + RAD_ISO_TILE_HEIGHT / 2 + RAD_ISO_TILE_HEIGHT
    );
    SDL_RenderDrawLine(
        renderer, 
        screen_x + RAD_ISO_TILE_WIDTH / 2, screen_y + RAD_ISO_TILE_HEIGHT, 
        screen_x + RAD_ISO_TILE_WIDTH / 2, screen_y + RAD_ISO_TILE_HEIGHT + RAD_ISO_TILE_HEIGHT
    );
    SDL_RenderDrawLine(
        renderer, 
        screen_x, screen_y + RAD_ISO_TILE_HEIGHT / 2, 
        screen_x + RAD_ISO_TILE_WIDTH / 2, screen_y + RAD_ISO_TILE_HEIGHT
    );
    SDL_RenderDrawLine(
        renderer, 
        screen_x + RAD_ISO_TILE_WIDTH, screen_y + RAD_ISO_TILE_HEIGHT / 2, 
        screen_x + RAD_ISO_TILE_WIDTH / 2, screen_y + RAD_ISO_TILE_HEIGHT
    );

    if(NULL != object->entity)
    {
        SDL_SetRenderDrawColor(renderer, 0, 255, 0, 255);
        SDL_Rect rect = {.x=screen_x,.y=screen_y-RAD_ISO_TILE_HEIGHT/2,.w=RAD_ISO_TILE_WIDTH,.h=RAD_ISO_TILE_HEIGHT};
        SDL_RenderFillRect(renderer, &rect);
    }
}
