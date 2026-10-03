#include <SDL2/SDL.h>
#include <emscripten/emscripten.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include <radish/rendering/iso_map.h>
#include <radish/rendering/iso_object.h>
#include <radish/io/net_codec.h>
#include <radish/io/net_event_handler.h>
#include <radish/io/net_request.h>
#include <radish/io/net_session.h>
#include <radish/events/event_manager.h>
#include <radish/view/user_input.h>
#include <radish/model/world.h>
#include <radish/rendering/game_events.h>
#include <radish/view/view.h>

#define WINDOW_WIDTH (SCREEN_WIDTH)
#define WINDOW_HEIGHT (SCREEN_HEIGHT)
#define MAX_INPUT_LENGTH 64
#define FONT_PATH "/assets/LiberationSansBold.ttf"
#define FONT_SIZE 16

typedef enum
{
    ZUC_STATE_CONNECTING = 0,
    ZUC_STATE_OPEN = 1,
    ZUC_STATE_CLOSED = 2
} ZucConnectionState;

///
/// Fenster, Renderer und Schrift (view/view.h). Nur ein Zeiger: RAD_CreateView
/// legt sie selbst an.
///
static RAD_RootView_t *view;

static ZucConnectionState connection_state = ZUC_STATE_CONNECTING;

static RAD_IsoMap_t *map = NULL;

static RAD_IoUserInput_t RAD_user_input;


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

///
/// Was der Client ueber seine Verbindung weiss (io/net_session.h): die Anfragen
/// schreiben hinein, die Abonnenten von net_event_manager lesen daraus.
///
static RAD_IoNetSession_t net_session = {
    .own_player_id = RAD_NET_USER_NONE,
    .last_discover_request = { .sent = false, .x = 0, .y = 0, .w = 0, .h = 0 }
};

///
/// Was die Abonnenten von net_event_manager brauchen (io/net_event_handler.h).
/// Die Zeiger setzt main().
///
static RAD_IoNetEventHandlerContext_t net_event_context = { .session = &net_session };

///
/// Setzt die eigene Spieler-Id aus der Kennung, die das Backend beim Verbinden
/// schickt ({"type": "identity"}, radish/backend/api/consumers.py). Gerufen von
/// der einbettenden Seite (frontend/src/app/shared/game-canvas/
/// game-canvas.component.ts) ueber Module.ccall. Eine Kennung, die keine ist,
/// laesst die bisherige Id stehen.
///
EMSCRIPTEN_KEEPALIVE
void RAD_ClientSetPlayerId(const char *identifier)
{
    const RAD_NetUserId_t id = RAD_NetUserIdFromIdentifier(identifier);
    if(id == RAD_NET_USER_NONE)
    {
        printf("Keine gueltige Spieler-Kennung: %s\n", identifier ? identifier : "(null)");
        return;
    }

    net_session.own_player_id = id;
    // Die Kommandos tragen sie mit, gelesen wird sie dort nicht (io/net_session.h).
    RAD_user_input.user = id;
    printf("Spieler-Id 0x%llx (%s)\n", (unsigned long long)id, identifier);
}

static bool RAD_IoUserinputSendCommandCallback(const RAD_NetMoveRequest_t *request)
{
    return RAD_IoNetRequestMove(&net_session, request);
}

///
/// Wird von der einbettenden Seite gerufen, sobald Bytes vom Server eintreffen
/// (frontend/.../game-canvas.component.ts, forwardToWasm), also nicht aus
/// RAD_EMCC_OnNewFrame(): die Antwort trifft irgendwann zwischen zwei Bildern
/// ein. Unterbrechen kann sie den Frame nicht -- JS ist einthreadig und
/// RAD_EMCC_OnNewFrame() laeuft durch.
///
EMSCRIPTEN_KEEPALIVE
void RAD_OnMessageReceived(const uint8_t *data, int length)
{
    if(length < 0)
    {
        return;
    }

    const RAD_NetCodecResult_t result = RAD_NetDispatchServerMessage(net_event_manager, data, (size_t)length);

    if(result != RAD_NET_CODEC_OK)
    {
        printf("<- %d Bytes verworfen: %s\n", length, RAD_NetCodecResultText(result));
    }
}

EMSCRIPTEN_KEEPALIVE
void zuc_on_connection_state(int state)
{
    connection_state = (ZucConnectionState)state;
    printf("%s\n", state == ZUC_STATE_OPEN ? "[Verbindung offen]" :
                    state == ZUC_STATE_CLOSED ? "[Verbindung getrennt]" : "[Verbinde...]");

    // Sobald die Verbindung offen ist, den Ausschnitt anfragen, den die Kamera
    // zeigt -- auch nach einem Neuaufbau, dann braucht der Client den aktuellen
    // Stand. Erst hier und nicht in main(): main() laeuft, bevor die einbettende
    // Seite Module.sendToChannel setzt (frontend/.../game-canvas.component.ts),
    // eine Anfrage von dort wuerde verworfen.
    if(connection_state != ZUC_STATE_OPEN || map == NULL)
    {
        return;
    }

    int32_t x = 0, y = 0, w = 0, h = 0;
    RAD_IsoMapVisibleArea(map, WINDOW_WIDTH, WINDOW_HEIGHT, &x, &y, &w, &h);
    if(w <= 0 || h <= 0)
    {
        printf("Kein Discover: die Kamera zeigt nichts vom Raster.\n");
    }
    else
    {
        RAD_IoNetRequestDiscover(&net_session, (uint32_t)x, (uint32_t)y, (uint32_t)w, (uint32_t)h);
    }

    // Danach die Reserven -- unabhaengig von der Kamera, also auch ohne Discover.
    RAD_IoNetRequestDiscoverReserve();
}

static void RAD_EMCC_OnNewFrame(void)
{
    RAD_UpdateView(view);

    // Nur bei offener Verbindung: sonst wirft Module.sendToChannel die Bytes
    // stillschweigend weg (einbettende Seite), und die Sequenznummern haetten Luecken,
    // die nach Verlust auf der Strecke aussehen.
    if(connection_state == ZUC_STATE_OPEN)
    {
        //send_test_move_command();
    }
}

int main(void)
{
    setvbuf(stdout, NULL, _IOLBF, 0);

    RAD_ClientWorldInit(&world);

    // Vor der RootView: ihre IsoMapView zeichnet die Map.
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

    net_event_context.world = &world;
    net_event_context.user_input = &RAD_user_input;

    RAD_NetEventManagerSubscribeToCommandResponseEvents(net_event_manager, (RAD_NetEventsCommandResponseCallback_t){
        .user_argument = &net_event_context,
        .received = RAD_IoNetOnCommandResponse
    });
    RAD_NetEventManagerSubscribeToGameEvents(net_event_manager, (RAD_NetEventsGameCallback_t){
        .user_argument = &net_event_context,
        .created = RAD_IoNetOnGameCreated,
        .finished = RAD_IoNetOnGameFinished,
        .current_player = RAD_IoNetOnCurrentPlayer,
        .players = RAD_IoNetOnPlayers,
        .world_size = RAD_IoNetOnWorldSize,
        .tiles = RAD_IoNetOnTiles,
        .reserve_unit = RAD_IoNetOnReserveUnit
    });
    RAD_NetEventManagerSubscribeToTileEvents(net_event_manager, (RAD_NetEventsTileCallback_t){
        .user_argument = &net_event_context,
        .created = RAD_IoNetOnTileCreated,
        .removed = RAD_IoNetOnTileRemoved,
        .changed = RAD_IoNetOnTileChanged
    });

    // Vor der RootView: sie ruft ihn bei jedem Linksklick auf.
    RAD_user_input = RAD_CreateIoUserInputState(&world, net_session.own_player_id, RAD_IoUserinputSendCommandCallback);

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

    view = RAD_CreateView("Zucchini Client", WINDOW_WIDTH, WINDOW_HEIGHT, FONT_PATH, FONT_SIZE, map, &RAD_user_input, &world);
    if(view == NULL)
    {
        printf("Keine View -- Abbruch.\n");
        return 1;
    }

    printf("Zucchini-Client gestartet.\n");

    emscripten_set_main_loop(RAD_EMCC_OnNewFrame, 0, 1);

    RAD_DestroyView(&view);
    RAD_DestroyNetEventManager(&net_event_manager);

    printf("Bye Bye!\n");
    return 0;
}
