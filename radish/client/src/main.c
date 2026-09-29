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
#include <radish/io/net_codec.h>
#include <radish/events/event_manager.h>
#include <radish/io/user_input.h>
#include <radish/io/camera_control.h>
#include <radish/model/world.h>
#include <radish/rendering/game_events.h>

#define ZUC_HEADER_SIZE 8
#define ZUC_CODE 1u

///
/// Uuid des Benutzers, der hier spielt. Sie geht in jedes Kommando
/// (RAD_NetMoveRequest_t.user), und der Server erkennt daran, wer gesendet hat.
///
/// Fest verdrahtet, weil es keine Anmeldung gibt: es gibt noch nichts, was eine
/// Uuid ausstellen koennte. Ein zweiter Browser traegt damit dieselbe -- fuer den
/// Server sind beide derselbe Benutzer. Das aendert sich, sobald die Uuid von
/// aussen kommt; bis dahin steht sie an genau dieser Stelle und nirgends sonst.
///
#define RAD_CLIENT_USER_ID ((RAD_NetUserId_t)0x1)

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

static RAD_IoCameraControl_t RAD_camera_control;

///
/// Das Iso-Objekt, das gerade den Fokus traegt, oder NULL. Gemerkt statt aus der
/// letzten Mausposition zurueckgerechnet: verschiebt sich die Kamera, liegt unter
/// der alten Position ein anderes Feld.
///
static RAD_IsoObject_t *focused_object = NULL;

///
/// Was der Client von der Welt weiss (model/world.h), gefuellt aus dem, was der
/// Server schickt.
///
static RAD_ClientWorld_t world;

///
/// Verteilt, was der Server ueber die Verbindung schickt
/// (events/event_manager.h). Nur ein Zeiger: RAD_CreateNetEventManager legt ihn
/// selbst an.
///
static RAD_NetEventManager_t *net_event_manager;


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
static RAD_NetSequence_t awaiting_sequence = 0;

///
/// Obergrenze der gepackten NetUserRequest (net_codec.h/net_codec.c), davor die
/// acht Byte des Codefeldes. Ein Zug mit allen RAD_NET_PATH_MAX_STEPS Feldern
/// bleibt mit Protobufs Varint-Kodierung deutlich darunter; 128 sind reichlich
/// und ersparen es, die Groesse nachzurechnen. Reicht der Puffer einmal nicht,
/// meldet RAD_NetEncodeMoveRequest das ueber
/// RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL, statt still abzuschneiden.
///
#define ZUC_COMMAND_MESSAGE_MAX 128

static bool RAD_SendCommandToServer(const RAD_NetMoveRequest_t *request)
{
    uint8_t message[ZUC_HEADER_SIZE + ZUC_COMMAND_MESSAGE_MAX];
    encode_big_endian_uint64(message, ZUC_CODE);

    // Das Codefeld steht davor (siehe oben); dahinter schreibt
    // net_codec.h/net_codec.c die per Protobuf gepackte NetUserRequest.
    size_t payload_length = 0;
    const RAD_NetCodecResult_t result = RAD_NetEncodeMoveRequest(
        request, message + ZUC_HEADER_SIZE, ZUC_COMMAND_MESSAGE_MAX, &payload_length);

    if(result != RAD_NET_CODEC_OK)
    {
        printf("Kommando nicht verschickt: %s\n", RAD_NetCodecResultText(result));
        return false;
    }

    zuc_js_send(message, (int)(ZUC_HEADER_SIZE + payload_length));

    last_send_time_ms = emscripten_get_now();
    awaiting_sequence = request->sequence;

    printf("-> #%llu move\n", (unsigned long long)request->sequence);
    return true;
}

///
/// Die letzte Discover-Anfrage dieses Clients, damit das Log der Antwort sagt,
/// worauf sie antwortet (RAD_PrintDiscoverAnswerPrefix).
///
/// Die Antwort selbst nennt ihre Anfrage nicht -- auf der Strecke sind es drei
/// Spielereignisse ohne Bezug (protobuf/game.proto). Und der Server schickt sie
/// an jeden Client, nicht nur an den fragenden (game-server-core/src/main.c,
/// send_discover_events). Eine Antwort kann also auch die auf die Anfrage eines
/// anderen sein; das Log nennt deshalb die *letzte eigene* Anfrage und keine
/// sichere Zuordnung.
///
typedef struct
{
    bool sent;
    uint32_t x;
    uint32_t y;
    uint32_t w;
    uint32_t h;
} RAD_DiscoverRequest_t;

static RAD_DiscoverRequest_t last_discover_request = { .sent = false, .x = 0, .y = 0, .w = 0, .h = 0 };

///
/// Der Anfang jeder Logzeile einer Discover-Antwort. Ohne eigene Anfrage steht
/// das dabei, statt einen Ausschnitt zu nennen, nach dem niemand gefragt hat.
///
static void RAD_PrintDiscoverAnswerPrefix(void)
{
    if(!last_discover_request.sent)
    {
        printf("<- Discover-Antwort (ohne eigene Anfrage): ");
        return;
    }

    printf("<- Discover-Antwort auf x=%u y=%u w=%u h=%u: ",
        (unsigned)last_discover_request.x, (unsigned)last_discover_request.y,
        (unsigned)last_discover_request.w, (unsigned)last_discover_request.h);
}

///
/// Schickt eine Discover-Anfrage fuer den Ausschnitt (x, y) mit Breite "w" und
/// Hoehe "h" an den Server. Derselbe Rahmen wie bei einem Kommando: Codefeld
/// davor, dahinter die gepackte NetUserRequest.
///
/// EMSCRIPTEN_KEEPALIVE, solange es im Client noch keinen Ausloeser gibt: so
/// laesst sie sich von der einbettenden Seite oder aus der Browser-Konsole rufen
/// (Module._RAD_SendDiscoverToServer), und der Linker wirft sie nicht weg.
///
EMSCRIPTEN_KEEPALIVE
void RAD_SendDiscoverToServer(uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
    uint8_t message[ZUC_HEADER_SIZE + ZUC_COMMAND_MESSAGE_MAX];
    encode_big_endian_uint64(message, ZUC_CODE);

    size_t payload_length = 0;
    const RAD_NetCodecResult_t result = RAD_NetEncodeDiscover(
        x, y, w, h, message + ZUC_HEADER_SIZE, ZUC_COMMAND_MESSAGE_MAX, &payload_length);

    if(result != RAD_NET_CODEC_OK)
    {
        printf("Discover nicht verschickt: %s\n", RAD_NetCodecResultText(result));
        return;
    }

    zuc_js_send(message, (int)(ZUC_HEADER_SIZE + payload_length));

    last_discover_request = (RAD_DiscoverRequest_t){ .sent = true, .x = x, .y = y, .w = w, .h = h };

    printf("-> Discover: Ausschnitt x=%u y=%u w=%u h=%u\n", (unsigned)x, (unsigned)y, (unsigned)w, (unsigned)h);
}

static bool RAD_IoUserinputSendCommandCallback(const RAD_NetMoveRequest_t *request)
{
    return RAD_SendCommandToServer(request);
}

static void RAD_HandleMouseClick(const SDL_MouseButtonEvent *event, RAD_IoUserInput_t *user_input)
{
    int32_t x = 0, y = 0;
    RAD_ToFlatCoordinates(map, event->x, event->y, &x, &y);
    
    printf("Mouse Event %i, %i\n", x, y);
    RAD_IoUserInputOnLeftClick(user_input, x, y);
}

///
/// Abonnenten von net_event_manager (events/event_manager.h). Der Manager hat
/// je Gruppe nur einen Abonnenten -- hier wird deshalb verteilt: ins Log, in
/// den Weltzustand (world) und in die Darstellung (map).
///
static void RAD_OnNetCommandResponseReceived(void *user_argument, const RAD_NetCommandResponse_t *response)
{
    (void)user_argument;

    if(response->sequence == awaiting_sequence)
    {
        printf("<- #%llu Antwort: %s (%.1f ms)\n",
               (unsigned long long)response->sequence,
               response->success ? "ausgefuehrt" : "abgelehnt",
               emscripten_get_now() - last_send_time_ms);
    }
    else
    {
        printf("<- #%llu Antwort: %s\n",
               (unsigned long long)response->sequence,
               response->success ? "ausgefuehrt" : "abgelehnt");
    }

    // Der Server schickt die geaenderten Felder nicht als Tile-Ereignis mit --
    // einen bestaetigten Zug traegt der Client deshalb selbst nach, bevor der
    // User-Input davon erfaehrt.
    if(response->success)
    {
        RAD_ClientWorldMoveEntity(&world, response->entity, &response->path);
        RAD_IsoMapMoveEntity(map, response->entity, &response->path);
    }

    RAD_IoUserInputOnCommandResponseReceived(&RAD_user_input, response);
}

static void RAD_OnNetGameCreated(void *user_argument)
{
    (void)user_argument;
    printf("<- Spiel erstellt\n");
}

static void RAD_OnNetGameFinished(void *user_argument)
{
    (void)user_argument;
    printf("<- Spiel beendet\n");
}

static void RAD_OnNetCurrentPlayer(void *user_argument, uint64_t user_id)
{
    (void)user_argument;
    RAD_PrintDiscoverAnswerPrefix();
    if(user_id == 0)
    {
        printf("am Zug = niemand\n");
        return;
    }
    printf("am Zug = 0x%llx\n", (unsigned long long)user_id);
}

static void RAD_OnNetPlayers(void *user_argument, const uint64_t *user_ids, size_t number_of_users)
{
    (void)user_argument;
    RAD_PrintDiscoverAnswerPrefix();
    printf("Mitspieler (%zu) =", number_of_users);
    if(number_of_users == 0)
    {
        printf(" keine");
    }
    for(size_t i = 0; i < number_of_users; ++i)
    {
        printf(" 0x%llx", (unsigned long long)user_ids[i]);
    }
    printf("\n");
}

static void RAD_OnNetWorldSize(void *user_argument, uint32_t width, uint32_t height)
{
    (void)user_argument;
    RAD_ClientWorldSetSize(&world, width, height);
    RAD_PrintDiscoverAnswerPrefix();
    printf("Spielfeld = %u x %u Felder\n", (unsigned)width, (unsigned)height);
}

///
/// Ein Feld, wie es der Server schickt, in Weltzustand und Darstellung.
///
static void RAD_ApplyNetTile(const RAD_NetTile_t *tile)
{
    RAD_ClientWorldApplyTile(&world, tile);
    RAD_IsoMapApplyTile(map, tile);
}

///
/// Die Attribute eines Feldes in einer Form, ohne Zeilenende -- fuer die
/// Discover-Antwort und die Tile-Ereignisse gleich, damit ein Feld im Log immer
/// gleich aussieht.
///
static void RAD_PrintNetTile(const RAD_NetTile_t *tile)
{
    printf("Feld (%u, %u) Typ=%s z=%u Figur=",
        (unsigned)tile->x, (unsigned)tile->y, RAD_NetTileTypeText(tile->type), (unsigned)tile->z);

    // -1 ist RAD_NET_ENTITY_NONE; jede andere Zahl ist eine Figur, auch die 0.
    if(tile->entity_id == RAD_NET_ENTITY_NONE)
    {
        printf("keine");
    }
    else
    {
        printf("%d", (int)tile->entity_id);
    }
}

static void RAD_OnNetTiles(void *user_argument, const RAD_NetTile_t *tiles, size_t number_of_tiles)
{
    (void)user_argument;

    if(number_of_tiles == 0)
    {
        RAD_PrintDiscoverAnswerPrefix();
        printf("keine Felder\n");
        return;
    }

    for(size_t i = 0; i < number_of_tiles; ++i)
    {
        RAD_PrintDiscoverAnswerPrefix();
        RAD_PrintNetTile(&tiles[i]);
        printf("\n");
        RAD_ApplyNetTile(&tiles[i]);
    }
}

static void RAD_OnNetTileCreated(void *user_argument, const RAD_NetTile_t *tile)
{
    (void)user_argument;
    printf("<- Tile erstellt: ");
    RAD_PrintNetTile(tile);
    printf("\n");
    RAD_ApplyNetTile(tile);
}

static void RAD_OnNetTileRemoved(void *user_argument, uint32_t x, uint32_t y)
{
    (void)user_argument;
    printf("<- Tile entfernt (%u, %u)\n", x, y);
    RAD_ClientWorldRemoveTile(&world, x, y);
    RAD_IsoMapRemoveTile(map, x, y);
}

static void RAD_OnNetTileChanged(void *user_argument, const RAD_NetTile_t *tile)
{
    (void)user_argument;
    printf("<- Tile geaendert: ");
    RAD_PrintNetTile(tile);
    printf("\n");
    RAD_ApplyNetTile(tile);
}

///
/// Wird von der einbettenden Seite gerufen, sobald Bytes vom Server eintreffen
/// (frontend/.../game-canvas.component.ts, forwardToWasm), also nicht aus
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

    // Die Bytes sind eine per Protobuf gepackte NetServerMessage
    // (net_codec.h). RAD_NetDispatchServerMessage entpackt sie einmal und
    // veroeffentlicht das Ergebnis ueber net_event_manager -- die Abonnenten
    // oben werten es aus, hier gibt es nur noch den Fehlerfall zu loggen.
    const RAD_NetCodecResult_t result = RAD_NetDispatchServerMessage(net_event_manager, data, (size_t)length);

    if(result != RAD_NET_CODEC_OK)
    {
        printf("<- %d Bytes verworfen: %s\n", length, RAD_NetCodecResultText(result));
    }
}

///
/// Ob nach dem Beitritt die erste Discover-Anfrage noch aussteht. Gesetzt, sobald
/// die Verbindung aufgeht -- auch nach einem Wiederverbinden --, und im naechsten
/// Frame eingeloest (frame()). Nicht gleich im Callback: der kommt von der
/// einbettenden Seite und kann eintreffen, bevor main() die Iso-Map angelegt hat,
/// deren Kamera den Ausschnitt bestimmt.
///
static bool discover_pending = false;

EMSCRIPTEN_KEEPALIVE
void zuc_on_connection_state(int state)
{
    connection_state = (ZucConnectionState)state;
    discover_pending = (connection_state == ZUC_STATE_OPEN);
    printf("%s\n", state == ZUC_STATE_OPEN ? "[Verbindung offen]" :
                    state == ZUC_STATE_CLOSED ? "[Verbindung getrennt]" : "[Verbinde...]");
}

///
/// Die Kamera zeigt einen anderen Ausschnitt (io/camera_control.h): im naechsten
/// Frame beim Server anfragen, wie beim Beitritt.
///
static void RAD_OnCameraViewChanged(void *user_argument)
{
    (void)user_argument;
    discover_pending = true;
}

///
/// Setzt den Fokus auf das Feld unter dem Mauszeiger. Ausserhalb des Rasters
/// traegt ihn keines.
///
static void RAD_UpdateFocus(int32_t screen_x, int32_t screen_y)
{
    if(focused_object != NULL)
    {
        focused_object->focus = false;
    }

    focused_object = RAD_IsoObjectAtScreenCoordinates(map, screen_x, screen_y);

    if(focused_object != NULL)
    {
        focused_object->focus = true;
    }
}

static void handle_events(void)
{
    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        // Zuerst die Kamera: rechte Maustaste und Pfeiltasten gehoeren ihr.
        if(RAD_IoCameraControlHandleEvent(&RAD_camera_control, &event))
        {
            continue;
        }

        switch(event.type)
        {
            case SDL_MOUSEBUTTONUP:
                if(event.button.button == SDL_BUTTON_LEFT)
                {
                    RAD_HandleMouseClick(&event.button, &RAD_user_input);
                }
                break;
            case SDL_MOUSEMOTION:
                RAD_UpdateFocus(event.motion.x, event.motion.y);
                break;
            default:
                break;
        }
    }
}

static void frame(void)
{
    // Vor handle_events: ein Klick im selben Frame wuerde sonst ein Kommando vor
    // die Discover-Anfrage stellen, und sie soll nach dem Beitritt die erste sein.
    if(discover_pending && (connection_state == ZUC_STATE_OPEN) && (map != NULL))
    {
        int32_t x = 0, y = 0, w = 0, h = 0;
        RAD_IsoMapVisibleArea(map, WINDOW_WIDTH, WINDOW_HEIGHT, &x, &y, &w, &h);
        RAD_SendDiscoverToServer((uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h);
        discover_pending = false;
    }

    handle_events();

    // Nur bei offener Verbindung: sonst wirft Module.sendToChannel die Bytes
    // stillschweigend weg (einbettende Seite), und die Sequenznummern haetten Luecken,
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

    RAD_ClientWorldInit(&world);

    // Vor dem Net-Event-Manager: seine Abonnenten schreiben in die Map.
    map = RAD_CreateIsoMap();
    if(map == NULL)
    {
        printf("Keine Iso-Map -- Abbruch.\n");
        return 1;
    }

    net_event_manager = RAD_CreateNetEventManager();
    if(net_event_manager == NULL)
    {
        printf("Kein Net-Event-Manager -- Abbruch.\n");
        return 1;
    }

    RAD_NetEventManagerSubscribeToCommandResponseEvents(net_event_manager, (RAD_NetEventsCommandResponseCallback_t){
        .user_argument = NULL,
        .received = RAD_OnNetCommandResponseReceived
    });
    RAD_NetEventManagerSubscribeToGameEvents(net_event_manager, (RAD_NetEventsGameCallback_t){
        .user_argument = NULL,
        .created = RAD_OnNetGameCreated,
        .finished = RAD_OnNetGameFinished,
        .current_player = RAD_OnNetCurrentPlayer,
        .players = RAD_OnNetPlayers,
        .world_size = RAD_OnNetWorldSize,
        .tiles = RAD_OnNetTiles
    });
    RAD_NetEventManagerSubscribeToTileEvents(net_event_manager, (RAD_NetEventsTileCallback_t){
        .user_argument = NULL,
        .created = RAD_OnNetTileCreated,
        .removed = RAD_OnNetTileRemoved,
        .changed = RAD_OnNetTileChanged
    });


    SDL_Init(SDL_INIT_VIDEO);
    TTF_Init();

    window = SDL_CreateWindow("Zucchini Client", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                               WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    font = TTF_OpenFont("/assets/LiberationSansBold.ttf", 16);

    SDL_StartTextInput();
    printf("Zucchini-Client gestartet.\n");

    RAD_user_input = RAD_CreateIoUserInputState(&world, RAD_CLIENT_USER_ID, RAD_IoUserinputSendCommandCallback);

    RAD_IoUserinputMoveActionCallbacks_t move_event_callbacks = {
        .started=RAD_RenderingOnMoveActionStarted,
        .waypoint_added=RAD_RenderingOnMoveActionWaypointAdded,
        .waypoint_rejected=RAD_RenderingOnMoveActionWaypointRejected,
        .accepted=RAD_RenderingOnMoveActionAccepted,
        .action_requested=RAD_RenderingOnMoveActionRequested,
        .response_received=RAD_RenderingOnMoveActionResponseReceived,
        .finished=RAD_RenderingOnMoveActionFinished
    };
    RAD_IoUserinputSubscribeToMoveActionEvents(&RAD_user_input, map, move_event_callbacks);

    RAD_camera_control = RAD_CreateIoCameraControl(map, WINDOW_WIDTH, WINDOW_HEIGHT);
    RAD_IoCameraControlSubscribeToViewChanged(&RAD_camera_control, NULL, RAD_OnCameraViewChanged);

    emscripten_set_main_loop(frame, 0, 1);

    RAD_DestroyNetEventManager(&net_event_manager);

    printf("Bye Bye!\n");
    return 0;
}
