#include "SDL2/SDL_events.h"
#include "SDL2/SDL_scancode.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <emscripten/emscripten.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <radish/rendering/iso_map.h>
#include <radish/rendering/iso_object.h>
#include <radish/game/game.h>
#include <radish/serialization/serialization.h>
#include <radish/game/events/event_manager.h>


#define ZUC_HEADER_SIZE 8
#define ZUC_CODE 1u

#define WINDOW_WIDTH (SCREEN_WIDTH)
#define WINDOW_HEIGHT (SCREEN_HEIGHT)
#define MAX_INPUT_LENGTH 64

typedef enum
{
    ZUC_STATE_CONNECTING = 0,
    ZUC_STATE_OPEN = 1,
    ZUC_STATE_CLOSED = 2
} ZucConnectionState;

static SDL_Window *window;
static SDL_Renderer *renderer;
static TTF_Font *font;

static char input_buffer[MAX_INPUT_LENGTH + 1] = {0};
static int input_length = 0;

static ZucConnectionState connection_state = ZUC_STATE_CONNECTING;

static RAD_IsoMap_t *map = NULL;

static RAD_Game_t *game;
static RAD_EventManager_t event_manager;

///
/// Startwelt als JSON. Ausgeschrieben waeren das 64 Tile-Objekte; die Makros
/// falten sie zu einem Raster, das im Editor noch als Karte lesbar bleibt.
/// ZUC_TILE_ON setzt die Entitaets-Id auf dem Tile -- sie muss zu "x"/"y" der
/// Entitaet weiter unten passen, sonst lehnt der Loader die Datei ab.
///
#define ZUC_TILE(x, y, type)        "{\"x\":" #x ",\"y\":" #y ",\"type\":\"" type "\",\"entity\":null}"
#define ZUC_TILE_ON(x, y, type, id) "{\"x\":" #x ",\"y\":" #y ",\"type\":\"" type "\",\"entity\":" #id "}"
#define ZUC_ROW(y, t0, t1, t2, t3, t4, t5, t6, t7)   \
    "[" ZUC_TILE(0, y, t0) "," ZUC_TILE(1, y, t1) "," \
        ZUC_TILE(2, y, t2) "," ZUC_TILE(3, y, t3) "," \
        ZUC_TILE(4, y, t4) "," ZUC_TILE(5, y, t5) "," \
        ZUC_TILE(6, y, t6) "," ZUC_TILE(7, y, t7) "]"

static const char initial_world_json[] =
"{"
    "\"format\":\"radish-save\","
    "\"version\":1,"
    "\"game\":{\"world\":{"
        "\"width\":8,"
        "\"height\":8,"
        "\"tiles\":["
            ZUC_ROW(0, "ground","ground","ground","ground","ground","ground","ground","ground") ","
            ZUC_ROW(1, "ground","ground","water", "water", "ground","ground","ground","ground") ","
            ZUC_ROW(2, "ground","ground","water", "water", "ground","ground","ground","ground") ","
            ZUC_ROW(3, "ground","ground","ground","ground","ground","ground","ground","ground") ","
            // Zeile 4 traegt die beiden Entitaeten und ist deshalb ausgeschrieben.
            "[" ZUC_TILE(0, 4, "ground")       "," ZUC_TILE(1, 4, "ground") ","
                ZUC_TILE(2, 4, "ground")       "," ZUC_TILE_ON(3, 4, "ground", 0) ","
                ZUC_TILE(4, 4, "void")         "," ZUC_TILE_ON(5, 4, "ground", 1) ","
                ZUC_TILE(6, 4, "ground")       "," ZUC_TILE(7, 4, "ground") "],"
            ZUC_ROW(5, "ground","ground","ground","ground","ground","ground","ground","ground") ","
            ZUC_ROW(6, "ground","ground","ground","ground","ground","ground","ground","ground") ","
            ZUC_ROW(7, "ground","ground","ground","ground","ground","ground","ground","ground")
        "],"
        "\"entities\":["
            "{\"id\":0,\"type\":\"player\",\"x\":3,\"y\":4},"
            "{\"id\":1,\"type\":\"npc\",\"x\":5,\"y\":4}"
        "]"
    "}}"
"}";



///
/// Initialisiert ein bereits angelegtes Spiel aus JSON.
///
/// RAD_DeserializeGameFromJson kuemmert sich um alles Weitere: es laesst jsmn
/// zaehlen und parsen, prueft Format und Version, baut die Welt ueber die
/// regulaere Spawn-Schnittstelle wieder auf und uebernimmt sie erst, wenn alles
/// stimmt. Schlaegt das Laden fehl, bleibt "target" deshalb unveraendert.
///
static bool init_game_from_json(RAD_Game_t *target, const char *json)
{
    RAD_SerializeResult_t result = RAD_DeserializeGameFromJson(target, json, strlen(json));

    if(result != RAD_SERIALIZE_OK)
    {
        printf("Welt nicht geladen: %s\n", RAD_SerializeResultText(result));
        return false;
    }

    printf("Welt geladen: %dx%d, %d Entitaeten\n",
           RAD_WORLD_WIDTH, RAD_WORLD_HEIGHT, target->world.number_of_entities);
    return true;
}

static void encode_big_endian_uint64(uint8_t *out, uint64_t value)
{
    for(int i = 0; i < 8; ++i)
    {
        out[i] = (uint8_t)(value >> (8 * (7 - i)));
    }
}

EM_JS(void, zuc_js_send, (const uint8_t *data, int length), {
    if(Module.sendToChannel)
    {
        Module.sendToChannel(HEAPU8.slice(data, data + length));
    }
});

static double last_send_time_ms = 0.0;
static int awaiting_response = 0;

static void send_current_input(void)
{
    if(input_length == 0)
    {
        return;
    }

    uint8_t message[ZUC_HEADER_SIZE + MAX_INPUT_LENGTH];
    encode_big_endian_uint64(message, ZUC_CODE);
    memcpy(message + ZUC_HEADER_SIZE, input_buffer, input_length);
    zuc_js_send(message, ZUC_HEADER_SIZE + input_length);

    last_send_time_ms = emscripten_get_now();
    awaiting_response = 1;

    printf("-> %s\n", input_buffer);

    input_buffer[0] = '\0';
    input_length = 0;
}

EMSCRIPTEN_KEEPALIVE
void zuc_on_response(const uint8_t *data, int length)
{
    // "data" ist nicht nullterminiert; die Laenge kommt deshalb ueber die
    // Praezision von %.*s herein.
    if(awaiting_response)
    {
        double roundtrip_ms = emscripten_get_now() - last_send_time_ms;
        awaiting_response = 0;
        printf("<- %.*s (%.1f ms)\n", length, (const char *)data, roundtrip_ms);
    }
    else
    {
        printf("<- %.*s\n", length, (const char *)data);
    }
}

EMSCRIPTEN_KEEPALIVE
void zuc_on_connection_state(int state)
{
    connection_state = (ZucConnectionState)state;
    printf("%s\n", state == ZUC_STATE_OPEN ? "[Verbindung offen]" :
                    state == ZUC_STATE_CLOSED ? "[Verbindung getrennt]" : "[Verbinde...]");
}

static void render_text(const char *text, int x, int y, SDL_Color color)
{
    if(text[0] == '\0')
    {
        return;
    }
    SDL_Surface *surface = TTF_RenderText_Blended(font, text, color);
    if(!surface)
    {
        return;
    }
    SDL_Texture *texture = SDL_CreateTextureFromSurface(renderer, surface);
    SDL_Rect dest = { x, y, surface->w, surface->h };
    SDL_RenderCopy(renderer, texture, NULL, &dest);
    SDL_DestroyTexture(texture);
    SDL_FreeSurface(surface);
}

static void handle_events(void)
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        switch(event.type)
        {
            case SDL_KEYUP:
                switch(event.key.keysym.scancode)
                {
                    case SDL_SCANCODE_UP:
                        map->camera.y += 25;
                        break;
                    case SDL_SCANCODE_DOWN:
                        map->camera.y -= 25;
                        break;
                    case SDL_SCANCODE_LEFT:
                        map->camera.x += 25;
                        break;
                    case SDL_SCANCODE_RIGHT:
                        map->camera.x -= 25;
                        break;
                    default:
                        break;
                }
                break;
            case SDL_MOUSEMOTION:
                RAD_IsoObjectAtScreenCoordinates(
                    map, 
                    event.motion.x - event.motion.xrel,// - (RAD_ISO_TILE_WIDTH / 2) - MAP_RENDER_OFFSET_X, 
                    event.motion.y - event.motion.yrel// - (RAD_ISO_TILE_HEIGHT / 2) - MAP_RENDER_OFFSET_Y
                )->focus=false;
                RAD_IsoObjectAtScreenCoordinates(
                    map, 
                    event.motion.x,// - (RAD_ISO_TILE_WIDTH / 2) - MAP_RENDER_OFFSET_X, 
                    event.motion.y// - (RAD_ISO_TILE_HEIGHT / 2) - MAP_RENDER_OFFSET_Y
                )->focus=true;
                RAD_EventManagerPublishMouseMoved(&event_manager, event.motion.x, event.motion.y, 0, 0);
                break;
            case SDL_TEXTINPUT:
                if(input_length + (int)strlen(event.text.text) <= MAX_INPUT_LENGTH)
                {
                    strcat(input_buffer, event.text.text);
                    input_length = (int)strlen(input_buffer);
                }
                break;
            case SDL_KEYDOWN:
                if(event.key.keysym.sym == SDLK_BACKSPACE && input_length > 0)
                {
                    input_buffer[--input_length] = '\0';
                }
                else if(event.key.keysym.sym == SDLK_RETURN)
                {
                    send_current_input();
                }
                break;
            default:
                break;
        }
    }
}

static void frame(void)
{
    handle_events();

    // SDL_SetRenderDrawColor(renderer, 17, 17, 17, 255);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);

    /*
    SDL_Color status_color;
    const char *status_text;
    switch(connection_state)
    {
        case ZUC_STATE_OPEN:
            status_color = (SDL_Color){ 0, 200, 0, 255 };
            status_text = "Verbunden";
            break;
        case ZUC_STATE_CLOSED:
            status_color = (SDL_Color){ 200, 0, 0, 255 };
            status_text = "Getrennt";
            break;
        default:
            status_color = (SDL_Color){ 200, 160, 0, 255 };
            status_text = "Verbinde...";
            break;
    }
    */

    //SDL_Rect status_dot = { 16, 16, 16, 16 };
    //SDL_SetRenderDrawColor(renderer, status_color.r, status_color.g, status_color.b, 255);
    //SDL_RenderFillRect(renderer, &status_dot);
    //render_text(status_text, 40, 14, (SDL_Color){ 230, 230, 230, 255 });
    
    /*
    SDL_Rect input_box = { 16, WINDOW_HEIGHT - 40, WINDOW_WIDTH - 32, 26 };
    SDL_SetRenderDrawColor(renderer, 60, 60, 60, 255);
    SDL_RenderDrawRect(renderer, &input_box);
    render_text(input_buffer, input_box.x + 6, input_box.y + 4, (SDL_Color){ 255, 255, 255, 255 });
    */

    RAD_RenderIsoMap(renderer, map);

    SDL_RenderPresent(renderer);
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    // Das Spiel liegt als statische Struktur schon vor, ist aber nur
    // nullinitialisiert -- und eine Null in tile.entity hiesse "Entitaet 0 steht
    // hier". Erst in einen gueltigen Grundzustand bringen, dann aus JSON laden.
    //
    event_manager = RAD_CreateEventManager();
    //init_game_from_json(game, initial_world_json);

    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    window = SDL_CreateWindow("Zucchini Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    font = TTF_OpenFont("/assets/LiberationSansBold.ttf", 16);

    SDL_StartTextInput();
    printf("Zucchini-Client gestartet.\n");

    map = RAD_CreateIsoMap(&event_manager);
    game = RAD_CreateGame(&event_manager);

    emscripten_set_main_loop(frame, 0, 1);

    RAD_DestroyGame(&game);
    printf("Bye Bye!\n");
    return 0;
}
