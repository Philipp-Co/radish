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
#include <radish/game/control/command/codec.h>
#include <radish/game/control/command/response.h>
#include <radish/io/user_input.h>


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

static ZucConnectionState connection_state = ZUC_STATE_CONNECTING;

static RAD_IsoMap_t *map = NULL;

static RAD_IoUserInput_t RAD_user_input;

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

///
/// Zeitmessung fuer den Rundlauf: wann das letzte Kommando hinausging und welche
/// Sequenznummer es trug. Passt die Nummer der Antwort dazu, steht die Zeit mit im
/// Log.
///
/// Nur die letzte, nicht eine Tabelle: bei 60 Kommandos je Sekunde koennen mehrere
/// unterwegs sein, und dann trifft eine Antwort ein, deren Kommando schon zwei
/// weiter ist. Statt dafuer Buch zu fuehren, entfaellt die Zeit in dem Fall -- sie
/// waere sonst die Zeit eines anderen Kommandos.
///
static double last_send_time_ms = 0.0;
static RAD_CommandSequence_t awaiting_sequence = 0;

///
/// Ein Kommando ist hoechstens 21 Byte lang (move_entity, siehe codec.h), davor
/// die acht Byte des Codefeldes. 64 sind reichlich und ersparen es, die Groesse
/// bei jeder neuen Kommandoart nachzurechnen.
///
#define ZUC_COMMAND_MESSAGE_MAX 64

static void RAD_SendCommandToServer(const RAD_Command_t *command)
{
    uint8_t message[ZUC_HEADER_SIZE + ZUC_COMMAND_MESSAGE_MAX];
    encode_big_endian_uint64(message, ZUC_CODE);

    // Der Writer beginnt hinter dem Codefeld: das wertet zucchini_server selbst
    // aus und schneidet es ab, es gehoert nicht zum Kommando.
    RAD_ByteWriter_t writer;
    RAD_ByteWriterInit(&writer, message + ZUC_HEADER_SIZE, ZUC_COMMAND_MESSAGE_MAX);
    RAD_SerializeCommand(&writer, command);

    if(!RAD_ByteWriterOk(&writer))
    {
        printf("Kommando passt nicht in %d Bytes\n", ZUC_COMMAND_MESSAGE_MAX);
        return;
    }

    zuc_js_send(message, (int)(ZUC_HEADER_SIZE + writer.length));

    last_send_time_ms = emscripten_get_now();
    awaiting_sequence = command->header.sequence;

    printf("-> #%llu move_entity\n", (unsigned long long)command->header.sequence);
}

bool RAD_IoUserinputSendCommandCallback(const RAD_Command_t *command)
{
    RAD_SendCommandToServer(command);
    return true;
}

static void RAD_HandleMouseClick(const SDL_MouseButtonEvent *event, RAD_Game_t *game, RAD_IoUserInput_t *user_input)
{
    int32_t x = 0, y = 0;
    RAD_Command_t command;
    RAD_ToFlatCoordinates(map, event->x, event->y, &x, &y);
    
    printf("Mouse Event %i, %i\n", x, y);
    RAD_IoUserInputOnLeftClick(user_input, x, y);
}

///
/// Wird von index.html aus dem onmessage des DataChannels gerufen, also nicht aus
/// frame(): die Antwort trifft irgendwann zwischen zwei Bildern ein. Unterbrechen
/// kann sie den Frame nicht -- JS ist einthreadig und frame() laeuft durch.
///
EMSCRIPTEN_KEEPALIVE
void zuc_on_response(const uint8_t *data, int length)
{
    if(length < 0)
    {
        return;
    }

    RAD_ByteReader_t reader;
    RAD_ByteReaderInit(&reader, data, (size_t)length);

    RAD_CommandResponse_t response;
    const RAD_CommandCodecResult_t result = RAD_DeserializeCommandResponse(&reader, &response);

    if(result != RAD_COMMAND_CODEC_OK)
    {
        printf("<- %d Bytes verworfen: %s\n", length, RAD_CommandCodecResultText(result));
        return;
    }

    RAD_IoUserInputOnCommandResponseReceived(&RAD_user_input, &response);
    if(response.header.sequence == awaiting_sequence)
    {
        printf("<- #%llu Antwort: art=%d value=%u (%.1f ms)\n",
               (unsigned long long)response.header.sequence,
               (int)response.header.type,
               response.value,
               emscripten_get_now() - last_send_time_ms);
    }
    else
    {
        printf("<- #%llu Antwort: art=%d value=%u\n",
               (unsigned long long)response.header.sequence,
               (int)response.header.type,
               response.value);
    }
}

EMSCRIPTEN_KEEPALIVE
void zuc_on_connection_state(int state)
{
    connection_state = (ZucConnectionState)state;
    printf("%s\n", state == ZUC_STATE_OPEN ? "[Verbindung offen]" :
                    state == ZUC_STATE_CLOSED ? "[Verbindung getrennt]" : "[Verbinde...]");
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
            case SDL_MOUSEBUTTONUP:
                RAD_HandleMouseClick(&event.button, game, &RAD_user_input);
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
            default:
                break;
        }
    }
}

static void frame(void)
{
    handle_events();

    // Nur bei offener Verbindung: sonst wirft Module.sendToChannel die Bytes
    // stillschweigend weg (index.html), und die Sequenznummern haetten Luecken,
    // die nach Verlust auf der Strecke aussehen.
    if(connection_state == ZUC_STATE_OPEN)
    {
        //send_test_move_command();
    }

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderClear(renderer);
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
    RAD_user_input = RAD_CreateIoUserInputState(game, RAD_IoUserinputSendCommandCallback);

    emscripten_set_main_loop(frame, 0, 1);

    RAD_DestroyGame(&game);
    printf("Bye Bye!\n");
    return 0;
}
