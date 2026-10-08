#include <radish/view/view.h>
#include <radish/view/iso_map_view.h>
#include <radish/view/terrain_info_view.h>
#include <radish/view/unit_info_view.h>
#include <radish/view/context_menu/context_menu_view.h>
#include <radish/view/debug/debug_view.h>
#include <radish/view/end_turn_view.h>
#include <radish/io/camera_control.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


///
/// Groesse von TerrainInfoView und UnitInfoView (view/view.h): je so breit, in
/// Pixeln, und hoechstens so hoch, in Prozent der Zeichenflaeche -- beide passen
/// sich ihrem Inhalt an.
///
#define RAD_VIEW_INFO_WIDTH 200
#define RAD_VIEW_TERRAIN_INFO_MAX_HEIGHT_PERCENT 27
#define RAD_VIEW_UNIT_INFO_MAX_HEIGHT_PERCENT 100

///
/// Groesse der EndTurnView (view/view.h), in Pixeln: so breit, hoechstens so
/// breit wie die Zeichenflaeche, und so hoch.
///
#define RAD_VIEW_END_TURN_WIDTH 160
#define RAD_VIEW_END_TURN_HEIGHT 32

///
/// Wie weit die Balken der UnitInfoView reichen (RAD_ViewFillUnitInfo): das
/// Modell kennt fuer Ruestung, Staerke und die Werte einer Waffe keinen
/// Hoechstwert, also steht hier einer fuer alle.
///
#define RAD_VIEW_UNIT_ATTRIBUTE_SCALE 10

///
/// Breite der DebugView (view/view.h), in Pixeln, und wie lange jede Messung
/// der RootView laeuft, bevor sie ihr den Durchschnitt gibt, in Millisekunden
/// (RAD_ViewDebugMetrics_t).
///
#define RAD_VIEW_DEBUG_WIDTH 200
#define RAD_VIEW_MEASURE_INTERVAL_MS 1000

///
/// Wie lange nach RAD_CreateView "max update" noch nicht gemessen wird, in
/// Millisekunden: die ersten Frames sind langsamer -- Schrift, erste Texte,
/// der Browser uebersetzt noch -- und stuenden sonst fuer immer als Maximum da.
///
#define RAD_VIEW_MAX_UPDATE_DELAY_MS 5000

///
/// Groesse der UnitDeploymentView der ContextMenuView (view/view.h): so breit,
/// in Pixeln, hoechstens so breit wie die Zeichenflaeche, und so hoch wie ein
/// Drittel davon.
///
#define RAD_VIEW_UNIT_DEPLOYMENT_WIDTH 200

///
/// Groesse der ContextMenuView (view/view.h): Radius des Rings, in Pixeln.
/// Ihre Eintraege legt sie selbst an (view/context_menu/context_menu_view.h).
///
#define RAD_VIEW_CONTEXT_MENU_RADIUS 60


///
/// Was die RootView fuer die DebugView misst. Jede Messung laeuft fuer sich,
/// mit eigenem Intervall: nach RAD_VIEW_MEASURE_INTERVAL_MS gibt sie der
/// DebugView ihren Durchschnitt und beginnt neu.
///
/// "avg fps" (RAD_ViewMeasureFps): seit wann gezaehlt wird, in SDL-Ticks, und
/// wie viele Frames seitdem gezeichnet wurden.
///
typedef struct
{
    uint64_t interval_start;
    uint32_t frames;
} RAD_ViewFpsMetric_t;

///
/// "avg update" und "max update" (RAD_ViewStartUpdateTimeMeasurement,
/// RAD_ViewEndUpdateTimeMeasurement): seit wann gemessen wird, in SDL-Ticks;
/// wann die laufende Messung begonnen hat und wie lange alle seitdem beendeten
/// zusammen gedauert haben -- in Schritten von SDL_GetPerformanceCounter --
/// und wie viele das waren. Dazu ab wann, in SDL-Ticks, die laengste Messung
/// mitzaehlt, und die laengste seitdem: sie gehoert zu keinem Intervall und
/// wird nie zurueckgesetzt.
///
typedef struct
{
    uint64_t interval_start;
    uint64_t start;
    uint64_t total;
    uint32_t count;
    uint64_t max_start;
    uint64_t max;
} RAD_ViewUpdateTimeMetric_t;

typedef struct
{
    RAD_ViewFpsMetric_t fps;
    RAD_ViewUpdateTimeMetric_t update_time;
} RAD_ViewDebugMetrics_t;


///
/// Die Struktur steht hier und nicht im Header: an Fenster, Renderer und Schrift
/// soll niemand vorbei an RAD_CreateView/RAD_DestroyView herankommen.
///
struct RAD_RootView
{
    SDL_Window *window;
    SDL_Renderer *renderer;
    TTF_Font *font;
    RAD_IsoMapView_t *iso_map_view;
    RAD_TerrainInfoView_t *terrain_info_view;
    RAD_UnitInfoView_t *unit_info_view;
    RAD_ContextMenuView_t *context_menu_view;
    RAD_DebugView_t *debug_view;
    RAD_EndTurnView_t *end_turn_view;

    ///
    /// Was sie fuer die DebugView misst.
    ///
    RAD_ViewDebugMetrics_t debug_metrics;

    bool terrain_info_visible;
    bool unit_info_visible;

    ///
    /// Haelt keine eigenen Ressourcen (io/camera_control.h) -- abzuraeumen ist
    /// sie mit der RootView selbst.
    ///
    RAD_IoCameraControl_t camera_control;

    ///
    /// Gehoeren dem Aufrufer (view/view.h, RAD_CreateView).
    ///
    RAD_IsoMap_t *map;

    ///
    /// Die Verbindung zum Backend: die RootView liest daraus die eigene
    /// Spieler-Id (RAD_ViewHandleLeftClick), die ContextMenuView schickt
    /// darueber Kommandos. Gehoert dem Aufrufer.
    ///
    RAD_IoNetSession_t *session;

    ///
    /// Woher die Reserve des eigenen Spielers kommt (RAD_ViewHandleLeftClick).
    /// Gehoert dem Aufrufer.
    ///
    const RAD_ClientWorld_t *world;

    ///
    /// Das Feld unter dem Mauszeiger (model/focus.h); die IsoMapView schreibt
    /// es. Gehoert dem Aufrufer.
    ///
    const RAD_ClientFocus_t *focus;

    ///
    /// Welche Einheit die UnitInfoView gerade zeigt (RAD_ViewSyncUnitInfo); NULL
    /// fuer keine. Die RootView beobachtet sie, solange sie sie zeigt: meldet sie
    /// "changed", ist die Anzeige veraltet ("unit_info_stale"), meldet sie
    /// "removed", zeigt sie keine mehr. Gehoert der Welt.
    ///
    const RAD_ClientUnit_t *shown_unit;
    bool unit_info_stale;
};


static void RAD_ViewHandleEvent(RAD_RootView_t *view, const SDL_Event *event);
static void RAD_ViewHandleMouseMotion(RAD_RootView_t *view, const SDL_MouseMotionEvent *motion);
static void RAD_ViewHandleLeftClick(RAD_RootView_t *view);
static bool RAD_ViewPlaceContextMenu(RAD_RootView_t *view);
static void RAD_ViewUpdateContextMenu(RAD_RootView_t *view);
static const RAD_ClientUnit_t* RAD_ViewSelectedUnit(const RAD_RootView_t *view);
static void RAD_ViewSyncUnitInfo(RAD_RootView_t *view);
static void RAD_ViewShowUnit(RAD_RootView_t *view, const RAD_ClientUnit_t *unit);
static void RAD_ViewFillUnitInfo(RAD_UnitInfoView_t *unit_info_view, const RAD_ClientUnit_t *unit);
static void RAD_ViewOnShownUnitChanged(void *user_argument, const RAD_ClientUnit_t *unit);
static void RAD_ViewOnShownUnitRemoved(void *user_argument, const RAD_ClientUnit_t *unit);
static void RAD_ViewMeasureFps(RAD_ViewFpsMetric_t *fps, RAD_DebugView_t *debug_view);
static void RAD_ViewStartUpdateTimeMeasurement(RAD_ViewUpdateTimeMetric_t *update_time);
static void RAD_ViewEndUpdateTimeMeasurement(RAD_ViewUpdateTimeMetric_t *update_time, RAD_DebugView_t *debug_view);
static double RAD_ViewPerformanceCounterToMs(uint64_t counter);


RAD_RootView_t* RAD_CreateView(
    const char *title,
    int32_t width,
    int32_t height,
    const char *font_path,
    int32_t font_size,
    RAD_IsoMap_t *map,
    RAD_IoNetSession_t *session,
    const RAD_ClientWorld_t *world,
    RAD_ClientFocus_t *focus
)
{
    RAD_RootView_t *view = malloc(sizeof(struct RAD_RootView));
    if(view == NULL)
    {
        return NULL;
    }

    *view = (struct RAD_RootView){
        .window = NULL,
        .renderer = NULL,
        .font = NULL,
        .iso_map_view = NULL,
        .terrain_info_view = NULL,
        .unit_info_view = NULL,
        .context_menu_view = NULL,
        .debug_view = NULL,
        .end_turn_view = NULL,
        .debug_metrics = {
            .fps = { .interval_start = 0, .frames = 0 },
            .update_time = { .interval_start = 0, .start = 0, .total = 0, .count = 0, .max_start = 0, .max = 0 }
        },
        .terrain_info_visible = true,
        .unit_info_visible = true,
        .map = map,
        .session = session,
        .world = world,
        .focus = focus,
        .shown_unit = NULL,
        .unit_info_stale = false
    };

    if(SDL_Init(SDL_INIT_VIDEO) != 0)
    {
        printf("SDL_Init: %s\n", SDL_GetError());
        free(view);
        return NULL;
    }

    if(TTF_Init() != 0)
    {
        printf("TTF_Init: %s\n", TTF_GetError());
        SDL_Quit();
        free(view);
        return NULL;
    }

    view->window = SDL_CreateWindow(title, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                                    width, height, SDL_WINDOW_SHOWN);
    if(view->window == NULL)
    {
        printf("SDL_CreateWindow: %s\n", SDL_GetError());
        RAD_DestroyView(&view);
        return NULL;
    }

    view->renderer = SDL_CreateRenderer(view->window, -1, SDL_RENDERER_ACCELERATED);
    if(view->renderer == NULL)
    {
        printf("SDL_CreateRenderer: %s\n", SDL_GetError());
        RAD_DestroyView(&view);
        return NULL;
    }

    view->font = TTF_OpenFont(font_path, font_size);
    if(view->font == NULL)
    {
        printf("TTF_OpenFont %s: %s\n", font_path, TTF_GetError());
        RAD_DestroyView(&view);
        return NULL;
    }

    int drawable_width = 0, drawable_height = 0;
    SDL_GetWindowSizeInPixels(view->window, &drawable_width, &drawable_height);

    view->camera_control = RAD_CreateIoCameraControl(map, drawable_width, drawable_height);

    view->iso_map_view = RAD_CreateIsoMapView(0, 0, drawable_width, drawable_height, map, world, focus);
    if(view->iso_map_view == NULL)
    {
        printf("Keine IsoMapView\n");
        RAD_DestroyView(&view);
        return NULL;
    }

    // Beide am unteren Rand, die TerrainInfoView am linken, die UnitInfoView am
    // rechten. Ist die Zeichenflaeche schmaler als beide zusammen, teilen sie sich
    // ihre Breite, damit sie sich nicht ueberlappen.
    const int32_t info_width = (drawable_width >= 2 * RAD_VIEW_INFO_WIDTH) ? RAD_VIEW_INFO_WIDTH : drawable_width / 2;
    const int32_t terrain_info_max_height = (drawable_height * RAD_VIEW_TERRAIN_INFO_MAX_HEIGHT_PERCENT) / 100;

    view->terrain_info_view = RAD_CreateTerrainInfoView(
        0,
        drawable_height - terrain_info_max_height,
        info_width,
        terrain_info_max_height,
        view->font,
        focus
    );
    const int32_t unit_info_max_height = (drawable_height * RAD_VIEW_UNIT_INFO_MAX_HEIGHT_PERCENT) / 100;
    view->unit_info_view = RAD_CreateUnitInfoView(
        drawable_width - info_width,
        drawable_height - unit_info_max_height,
        info_width,
        unit_info_max_height,
        view->font
    );
    if(view->terrain_info_view == NULL || view->unit_info_view == NULL)
    {
        printf("Keine TerrainInfoView oder UnitInfoView\n");
        RAD_DestroyView(&view);
        return NULL;
    }

    // Unten mittig, zwischen TerrainInfoView und UnitInfoView.
    const int32_t end_turn_width = (drawable_width >= RAD_VIEW_END_TURN_WIDTH) ? RAD_VIEW_END_TURN_WIDTH : drawable_width;
    view->end_turn_view = RAD_CreateEndTurnView(
        (drawable_width - end_turn_width) / 2,
        drawable_height - RAD_VIEW_END_TURN_HEIGHT,
        end_turn_width,
        RAD_VIEW_END_TURN_HEIGHT,
        view->font,
        world,
        session
    );
    if(view->end_turn_view == NULL)
    {
        printf("Keine EndTurnView\n");
        RAD_DestroyView(&view);
        return NULL;
    }

    // Wo sie steht, setzt RAD_UpdateView jeden Frame neu (RAD_ViewUpdateContextMenu).
    // Ihre UnitDeploymentView ist ein Drittel so hoch wie die Zeichenflaeche.
    const int32_t unit_deployment_width = (drawable_width >= RAD_VIEW_UNIT_DEPLOYMENT_WIDTH) ? RAD_VIEW_UNIT_DEPLOYMENT_WIDTH : drawable_width;
    view->context_menu_view = RAD_CreateContextMenuView(
        0,
        0,
        RAD_VIEW_CONTEXT_MENU_RADIUS,
        view->font,
        unit_deployment_width,
        drawable_height / 3,
        session,
        map
    );
    if(view->context_menu_view == NULL)
    {
        printf("Keine ContextMenuView\n");
        RAD_DestroyView(&view);
        return NULL;
    }
    RAD_ContextMenuViewSetColor(view->context_menu_view, (SDL_Color){ 96, 96, 96, 160 });

    // In der linken oberen Ecke, hoechstens so breit wie die Zeichenflaeche;
    // die Hoehe folgt ihrem Inhalt.
    const int32_t debug_width = (drawable_width >= RAD_VIEW_DEBUG_WIDTH) ? RAD_VIEW_DEBUG_WIDTH : drawable_width;
    view->debug_view = RAD_CreateDebugView(
        0,
        0,
        debug_width,
        drawable_height,
        view->font
    );
    if(view->debug_view == NULL)
    {
        printf("Keine DebugView\n");
        RAD_DestroyView(&view);
        return NULL;
    }

    const uint64_t now = SDL_GetTicks64();
    view->debug_metrics.fps.interval_start = now;
    view->debug_metrics.update_time.interval_start = now;
    view->debug_metrics.update_time.max_start = now + RAD_VIEW_MAX_UPDATE_DELAY_MS;

    SDL_StartTextInput();

    return view;
}

///
/// Baut auch eine halb angelegte Komponente ab (RAD_CreateView im Fehlerfall):
/// SDL und SDL_ttf sind dann schon initialisiert, Fenster, Renderer, Schrift
/// oder eine der Views womoeglich noch NULL.
///
void RAD_DestroyView(RAD_RootView_t **view)
{
    RAD_RootView_t *v = *view;

    SDL_StopTextInput();

    if(v->debug_view != NULL)
    {
        RAD_DestroyDebugView(&v->debug_view);
    }
    if(v->context_menu_view != NULL)
    {
        RAD_DestroyContextMenuView(&v->context_menu_view);
    }
    if(v->end_turn_view != NULL)
    {
        RAD_DestroyEndTurnView(&v->end_turn_view);
    }
    // Die Welt lebt laenger: ohne Abmelden riefe die Einheit spaeter eine
    // RootView, die es nicht mehr gibt.
    RAD_ViewShowUnit(v, NULL);
    if(v->unit_info_view != NULL)
    {
        RAD_DestroyUnitInfoView(&v->unit_info_view);
    }
    if(v->terrain_info_view != NULL)
    {
        RAD_DestroyTerrainInfoView(&v->terrain_info_view);
    }
    if(v->iso_map_view != NULL)
    {
        RAD_DestroyIsoMapView(&v->iso_map_view);
    }
    if(v->font != NULL)
    {
        TTF_CloseFont(v->font);
    }
    if(v->renderer != NULL)
    {
        SDL_DestroyRenderer(v->renderer);
    }
    if(v->window != NULL)
    {
        SDL_DestroyWindow(v->window);
    }

    TTF_Quit();
    SDL_Quit();

    free(v);
    *view = NULL;
}

void RAD_UpdateView(RAD_RootView_t *view)
{
    RAD_ViewStartUpdateTimeMeasurement(&view->debug_metrics.update_time);

    SDL_Event event;
    while(SDL_PollEvent(&event))
    {
        RAD_ViewHandleEvent(view, &event);
    }

    SDL_SetRenderDrawColor(view->renderer, 0, 0, 0, 255);
    SDL_RenderClear(view->renderer);
    RAD_UpdateIsoMapView(view->iso_map_view, view->renderer);
    // Der Weg einer ziehenden Einheit liegt auf der Karte, unter allem anderen.
    RAD_UpdateContextMenuViewPathOverlay(view->context_menu_view, view->renderer);
    if(view->terrain_info_visible)
    {
        RAD_UpdateTerrainInfoView(view->terrain_info_view, view->renderer);
    }
    RAD_ViewSyncUnitInfo(view);
    if(view->unit_info_visible && view->shown_unit != NULL)
    {
        RAD_UpdateUnitInfoView(view->unit_info_view, view->renderer);
    }
    RAD_UpdateEndTurnView(view->end_turn_view, view->renderer);
    RAD_ViewUpdateContextMenu(view);
    RAD_UpdateDebugView(view->debug_view, view->renderer);
    SDL_RenderPresent(view->renderer);

    RAD_ViewMeasureFps(&view->debug_metrics.fps, view->debug_view);
    RAD_ViewEndUpdateTimeMeasurement(&view->debug_metrics.update_time, view->debug_view);
}

void RAD_ViewSetTerrainInfoVisible(RAD_RootView_t *view, bool visible)
{
    view->terrain_info_visible = visible;
}

void RAD_ViewSetUnitInfoVisible(RAD_RootView_t *view, bool visible)
{
    view->unit_info_visible = visible;
}

SDL_Renderer* RAD_ViewRenderer(const RAD_RootView_t *view)
{
    return view->renderer;
}

TTF_Font* RAD_ViewFont(const RAD_RootView_t *view)
{
    return view->font;
}


///
/// Ein SDL-Ereignis aus RAD_UpdateView (view/view.h).
///
static void RAD_ViewHandleEvent(RAD_RootView_t *view, const SDL_Event *event)
{
    // Zuerst die Kamera: rechte Maustaste und Pfeiltasten gehoeren ihr.
    if(RAD_IoCameraControlHandleEvent(&view->camera_control, event))
    {
        return;
    }

    // Dann die ContextMenuView: sie liegt ueber allem. Was einen ihrer
    // Eintraege oder ihre UnitDeploymentView trifft, gehoert ihr.
    if((event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP)
       && RAD_ViewPlaceContextMenu(view)
       && RAD_ContextMenuViewHandleMouseButton(view->context_menu_view, &event->button))
    {
        return;
    }

    // Dann die EndTurnView: was ihre Flaeche trifft, waehlt kein Feld aus.
    if((event->type == SDL_MOUSEBUTTONDOWN || event->type == SDL_MOUSEBUTTONUP)
       && RAD_EndTurnViewHandleMouseButton(view->end_turn_view, &event->button))
    {
        return;
    }

    if(event->type == SDL_MOUSEBUTTONUP && event->button.button == SDL_BUTTON_LEFT)
    {
        RAD_ViewHandleLeftClick(view);
    }
    else if(event->type == SDL_MOUSEMOTION)
    {
        RAD_ViewHandleMouseMotion(view, &event->motion);
    }

    // Alles andere wird vorerst verworfen.
}

///
/// Eine Mausbewegung aus RAD_ViewHandleEvent: geht an die ContextMenuView,
/// wenn sie zu sehen ist, und, wenn die sie nicht nimmt, an die IsoMapView.
/// Ueber einem Eintrag der ContextMenuView sieht die IsoMapView sie also auch,
/// ueber ihrer UnitDeploymentView oder waehrend die gezogen wird nicht. Die
/// Kamera-Steuerung hat sie vorher gesehen, auch waehrend sie die Kamera
/// zieht.
///
static void RAD_ViewHandleMouseMotion(RAD_RootView_t *view, const SDL_MouseMotionEvent *motion)
{
    if(RAD_ViewPlaceContextMenu(view)
       && RAD_ContextMenuViewHandleMouseMotion(view->context_menu_view, motion))
    {
        return;
    }
    RAD_IsoMapViewHandleMouseMotion(view->iso_map_view, motion);
}

///
/// Das Loslassen der linken Maustaste aus RAD_ViewHandleEvent. Meldet der
/// ContextMenuView das angeklickte Feld -- das im Fokus, die Maus steht ja
/// darauf (RAD_ViewHandleMouseMotion), NULL neben den Feldern. Ob sie sich
/// oeffnet oder schliesst, entscheidet ihre Zustandsmaschine
/// (view/context_menu/context_menu_state_machine.h), ebenso, welche Eintraege
/// sie zeigt und ueber welchem Feld sie steht. Oeffnet sie sich, gibt die
/// RootView ihr die Reserve.
///
static void RAD_ViewHandleLeftClick(RAD_RootView_t *view)
{
    RAD_ContextMenuView_t *menu = view->context_menu_view;
    const RAD_ClientTile_t *tile = view->focus->tile;

    const bool was_active = RAD_ContextMenuViewIsActive(menu);
    RAD_ContextMenuViewOnTileSelected(menu, tile);
    if(!was_active && RAD_ContextMenuViewIsActive(menu))
    {
        // Die Reserve des eigenen Spielers; NULL, solange der Server keine
        // Einheit von ihm geschickt hat. Neu geholt bei jedem Oeffnen.
        RAD_ContextMenuViewSetReserve(menu,
                                      RAD_ClientWorldReserve(view->world, view->session->own_player_id),
                                      RAD_ClientWorldUnits(view->world));
    }
}

///
/// Setzt die ContextMenuView mittig ueber ihr Feld
/// (RAD_ContextMenuViewAnchorTile), samt Kamera -- sie wandert also mit, wenn die Kamera sich bewegt. false,
/// wenn sie nicht zu sehen ist: unsichtbar (RAD_ContextMenuViewIsVisible),
/// oder ihr Feld wird nicht (mehr) gezeichnet; dann bleibt sie, wo sie war,
/// und bekommt weder Ereignisse noch wird sie gezeichnet. Sichtbar bleibt sie
/// dabei.
///
/// Vor jedem Ereignis und jedem Zeichnen zu rufen: die Kamera kann sich
/// dazwischen bewegt haben.
///
static bool RAD_ViewPlaceContextMenu(RAD_RootView_t *view)
{
    int32_t tile_x = 0, tile_y = 0, center_x = 0, center_y = 0;
    if(!RAD_ContextMenuViewAnchorTile(view->context_menu_view, &tile_x, &tile_y)
       || !RAD_IsoMapTileScreenCenter(view->map, tile_x, tile_y, &center_x, &center_y))
    {
        return false;
    }

    RAD_ContextMenuViewSetCenter(view->context_menu_view, center_x, center_y);
    return true;
}

///
/// Die Einheit auf dem ausgewaehlten Feld: in der ContextMenuView ist eins
/// ausgewaehlt (RAD_ContextMenuViewSelectedTile) -- auch, waehrend der Weg
/// einer Einheit zusammengeklickt wird --, und darauf steht eine. NULL sonst.
///
static const RAD_ClientUnit_t* RAD_ViewSelectedUnit(const RAD_RootView_t *view)
{
    int32_t tile_x = 0, tile_y = 0;
    if(!RAD_ContextMenuViewSelectedTile(view->context_menu_view, &tile_x, &tile_y))
    {
        return NULL;
    }

    const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt(view->world, tile_x, tile_y);
    return (tile != NULL) ? tile->unit : NULL;
}

///
/// Bringt die UnitInfoView auf die Einheit auf dem ausgewaehlten Feld
/// (RAD_ViewSelectedUnit). Jeden Frame vor dem Zeichnen zu rufen: zieht die
/// Einheit weg oder kommt eine an, folgt die UnitInfoView gleich. Neu gefuellt
/// wird sie nur, wenn es eine andere Einheit ist oder sich die gezeigte
/// geaendert hat.
///
static void RAD_ViewSyncUnitInfo(RAD_RootView_t *view)
{
    const RAD_ClientUnit_t *selected = RAD_ViewSelectedUnit(view);
    if(selected != view->shown_unit)
    {
        RAD_ViewShowUnit(view, selected);
    }

    if(view->unit_info_stale && view->shown_unit != NULL)
    {
        RAD_ViewFillUnitInfo(view->unit_info_view, view->shown_unit);
        view->unit_info_stale = false;
    }
}

///
/// Wechselt die gezeigte Einheit: meldet die RootView an der alten ab und an
/// der neuen an; NULL fuer keine. Die Anzeige gilt danach als veraltet.
///
/// Findet die Einheit keinen Platz fuer einen weiteren Beobachter, wird sie
/// trotzdem gezeigt -- dann eben so, wie sie beim Wechsel war.
///
static void RAD_ViewShowUnit(RAD_RootView_t *view, const RAD_ClientUnit_t *unit)
{
    if(view->shown_unit != NULL)
    {
        RAD_ClientUnitUnsubscribe(view->shown_unit, view);
    }

    view->shown_unit = unit;
    view->unit_info_stale = true;

    if(unit != NULL)
    {
        RAD_ClientUnitSubscribe(unit, (RAD_ClientUnitObserver_t){
            .user_argument = view,
            .changed = RAD_ViewOnShownUnitChanged,
            .removed = RAD_ViewOnShownUnitRemoved
        });
    }
}

static void RAD_ViewOnShownUnitChanged(void *user_argument, const RAD_ClientUnit_t *unit)
{
    (void)unit;
    RAD_RootView_t *view = (RAD_RootView_t*)user_argument;
    view->unit_info_stale = true;
}

///
/// Die gezeigte Einheit ist aus der Welt; abgemeldet ist die RootView damit
/// schon (model/unit.h).
///
static void RAD_ViewOnShownUnitRemoved(void *user_argument, const RAD_ClientUnit_t *unit)
{
    (void)unit;
    RAD_RootView_t *view = (RAD_RootView_t*)user_argument;
    view->shown_unit = NULL;
    view->unit_info_stale = true;
}

///
/// Fuellt "unit_info_view" mit "unit": Name, die Lebenspunkte der ganzen
/// Einheit (RAD_ClientUnitHitPoints), ihre Bewegungsreichweite und je Profil
/// ihrer Mitglieder eine Zeile. Was vorher dort stand, faellt weg.
///
/// **Eine Zeile je Profil**, in der Reihenfolge, in der es zuerst vorkommt, mit
/// der Zahl der Mitglieder dieses Profils. Die Werte sind die des ersten
/// Mitglieds mit dem Profil. Ruestung, Staerke und die Werte der Waffen reichen
/// bis RAD_VIEW_UNIT_ATTRIBUTE_SCALE; Widerstand kennt das Modell nicht, er
/// bleibt leer.
///
static void RAD_ViewFillUnitInfo(RAD_UnitInfoView_t *unit_info_view, const RAD_ClientUnit_t *unit)
{
    RAD_UnitInfoViewSetName(unit_info_view, unit->name);

    int32_t current = 0, maximum = 0;
    RAD_ClientUnitHitPoints(unit, &current, &maximum);
    RAD_UnitInfoViewSetHitPoints(unit_info_view, current, maximum);
    RAD_UnitInfoViewSetMovementRange(unit_info_view, unit->movement);

    RAD_UnitInfoViewClearEntities(unit_info_view);

    // Ein Profil zaehlt nur beim ersten Mitglied, das es traegt.
    for(uint32_t i = 0; i < unit->number_of_members; ++i)
    {
        const RAD_ClientMember_t *member = &unit->members[i];

        bool seen = false;
        for(uint32_t k = 0; k < i && !seen; ++k)
        {
            seen = (strcmp(unit->members[k].profile, member->profile) == 0);
        }
        if(seen)
        {
            continue;
        }

        uint32_t count = 0;
        for(uint32_t k = i; k < unit->number_of_members; ++k)
        {
            count += (strcmp(unit->members[k].profile, member->profile) == 0) ? 1 : 0;
        }

        const size_t number_of_weapons = (member->number_of_weapons < RAD_ENTITY_VIEW_WEAPONS_MAX)
            ? member->number_of_weapons : RAD_ENTITY_VIEW_WEAPONS_MAX;

        // Mehr Zeilen, als die UnitInfoView fasst: der Rest faellt weg.
        RAD_EntityView_t *entity = RAD_UnitInfoViewAddEntity(unit_info_view, count, number_of_weapons);
        if(entity == NULL)
        {
            break;
        }

        RAD_EntityViewSetName(entity, member->profile);
        RAD_EntityViewSetHitPoints(entity, member->health, member->health_max);
        RAD_EntityViewSetArmor(entity, member->armor, RAD_VIEW_UNIT_ATTRIBUTE_SCALE);
        RAD_EntityViewSetStrength(entity, member->strength, RAD_VIEW_UNIT_ATTRIBUTE_SCALE);

        for(size_t w = 0; w < number_of_weapons; ++w)
        {
            const RAD_ClientWeapon_t *weapon = &member->weapons[w];
            RAD_EntityViewSetWeaponName(entity, w, weapon->name);
            RAD_EntityViewSetWeaponStrength(entity, w, weapon->strength, RAD_VIEW_UNIT_ATTRIBUTE_SCALE);
            RAD_EntityViewSetWeaponPenetration(entity, w, weapon->penetration, RAD_VIEW_UNIT_ATTRIBUTE_SCALE);
            RAD_EntityViewSetWeaponRange(entity, w, weapon->max_range, RAD_VIEW_UNIT_ATTRIBUTE_SCALE);
            RAD_EntityViewSetWeaponNumberOfShots(entity, w, weapon->shots, RAD_VIEW_UNIT_ATTRIBUTE_SCALE);
        }
    }
}

///
/// Zeichnet die ContextMenuView, wenn sie zu sehen ist (RAD_ViewPlaceContextMenu).
///
static void RAD_ViewUpdateContextMenu(RAD_RootView_t *view)
{
    if(RAD_ViewPlaceContextMenu(view))
    {
        RAD_UpdateContextMenuView(view->context_menu_view, view->renderer);
    }
}


///
/// Zaehlt einen gezeichneten Frame; einmal je RAD_UpdateView, an dessen Ende.
/// Sind seit Beginn der Zaehlung RAD_VIEW_MEASURE_INTERVAL_MS vergangen,
/// bekommt "debug_view" den Durchschnitt darueber als "avg fps", und die
/// Zaehlung beginnt neu.
///
static void RAD_ViewMeasureFps(RAD_ViewFpsMetric_t *fps, RAD_DebugView_t *debug_view)
{
    fps->frames++;

    const uint64_t now = SDL_GetTicks64();
    const uint64_t elapsed = now - fps->interval_start;
    if(elapsed < RAD_VIEW_MEASURE_INTERVAL_MS)
    {
        return;
    }

    RAD_DebugViewSetAvgFps(debug_view, (float)((double)fps->frames * 1000.0 / (double)elapsed));
    fps->interval_start = now;
    fps->frames = 0;
}

///
/// Beginnt eine Messung der Ausfuehrungszeit -- am Anfang von RAD_UpdateView.
/// Beendet wird sie mit RAD_ViewEndUpdateTimeMeasurement.
///
static void RAD_ViewStartUpdateTimeMeasurement(RAD_ViewUpdateTimeMetric_t *update_time)
{
    update_time->start = SDL_GetPerformanceCounter();
}

///
/// Beendet die Messung aus RAD_ViewStartUpdateTimeMeasurement -- am Ende von
/// RAD_UpdateView, nach dem Anzeigen -- und zaehlt ihre Dauer mit. Zu sehen
/// ist alles ab dem naechsten Frame.
///
/// Ist sie laenger als alle bisher, bekommt "debug_view" sie sofort als "max
/// update" -- seit dem Start, ohne Intervall; die ersten
/// RAD_VIEW_MAX_UPDATE_DELAY_MS zaehlen dabei nicht mit, bis dahin bleibt dort
/// "-". Sind seit Beginn des Intervalls
/// RAD_VIEW_MEASURE_INTERVAL_MS vergangen, bekommt sie die durchschnittliche
/// Dauer darueber als "avg update", und das Intervall beginnt neu.
///
static void RAD_ViewEndUpdateTimeMeasurement(RAD_ViewUpdateTimeMetric_t *update_time, RAD_DebugView_t *debug_view)
{
    const uint64_t duration = SDL_GetPerformanceCounter() - update_time->start;
    update_time->total += duration;
    update_time->count++;

    const uint64_t now = SDL_GetTicks64();
    if(now >= update_time->max_start && duration > update_time->max)
    {
        update_time->max = duration;
        RAD_DebugViewSetMaxUpdateMs(debug_view, (float)RAD_ViewPerformanceCounterToMs(duration));
    }

    if(now - update_time->interval_start < RAD_VIEW_MEASURE_INTERVAL_MS)
    {
        return;
    }

    RAD_DebugViewSetAvgUpdateMs(debug_view, (float)(RAD_ViewPerformanceCounterToMs(update_time->total) / (double)update_time->count));
    update_time->interval_start = now;
    update_time->total = 0;
    update_time->count = 0;
}

///
/// Rechnet Schritte von SDL_GetPerformanceCounter in Millisekunden um.
///
static double RAD_ViewPerformanceCounterToMs(uint64_t counter)
{
    return (double)counter * 1000.0 / (double)SDL_GetPerformanceFrequency();
}
