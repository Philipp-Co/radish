#include <radish/view/context_menu/context_menu_view.h>
#include <radish/control/attack_unit.h>
#include <radish/control/deploy_unit.h>
#include <radish/control/move_unit.h>
#include <radish/control/terrain.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


///
/// In wie viele Stuecke der Ring zerlegt wird (RAD_ContextMenuViewFillRing).
/// Genug, dass er auch gross noch rund aussieht.
///
#define RAD_CONTEXT_MENU_VIEW_SEGMENTS 48

///
/// Wie breit der Ring nach RAD_CreateContextMenuView ist, in Pixeln.
///
#define RAD_CONTEXT_MENU_VIEW_DEFAULT_RING_WIDTH 24

///
/// Mit wie vielen Plaetzen fuer Eintraege die Liste anfaengt; reicht das nicht,
/// verdoppelt sie sich.
///
#define RAD_CONTEXT_MENU_VIEW_INITIAL_CAPACITY 4

///
/// Radius der Eintraege, in Pixeln (RAD_ContextMenuViewFillItems).
///
#define RAD_CONTEXT_MENU_VIEW_ITEM_RADIUS 80

///
/// Wo die Eintraege stehen, in Grad (0 oben, im Uhrzeigersinn). Schliessen
/// steht immer unten links. Rechts der erste mit seinem Mittelpunkt genau
/// oben auf dem Scheitel des Rings, jeder weitere so viel weiter im
/// Uhrzeigersinn, dass zwischen zwei Nachbarn RAD_CONTEXT_MENU_VIEW_ITEM_GAP
/// Pixel frei bleiben (RAD_ContextMenuViewItemStep).
///
#define RAD_CONTEXT_MENU_VIEW_CLOSE_ANGLE 225.0f
#define RAD_CONTEXT_MENU_VIEW_FIRST_RIGHT_ANGLE 0.0f

///
/// Wie viele Pixel zwischen zwei benachbarten Eintraegen frei bleiben, von
/// Rand zu Rand.
///
#define RAD_CONTEXT_MENU_VIEW_ITEM_GAP 32

///
/// Farben der Markierung des Weges (RAD_UpdateContextMenuViewOverlay):
/// seine Felder durchscheinend gelb, das Ziel kraeftiger.
///
#define RAD_CONTEXT_MENU_VIEW_PATH_COLOR ((SDL_Color){ 255, 220, 0, 90 })
#define RAD_CONTEXT_MENU_VIEW_PATH_TARGET_COLOR ((SDL_Color){ 255, 220, 0, 200 })

///
/// Farben der Markierung beim Angriff (RAD_UpdateContextMenuViewOverlay): die
/// Felder in Reichweite durchscheinend rot, das gewaehlte Ziel kraeftiger.
///
#define RAD_CONTEXT_MENU_VIEW_RANGE_COLOR ((SDL_Color){ 220, 40, 40, 70 })
#define RAD_CONTEXT_MENU_VIEW_RANGE_TARGET_COLOR ((SDL_Color){ 220, 40, 40, 190 })

///
/// Farben der Markierung beim Deployen (RAD_ContextMenuViewDrawDeployZone): die
/// Felder der eigenen Aufstellzone, auf die sich aufstellen laesst,
/// durchscheinend gruen, das ausgewaehlte Feld kraeftiger -- gruen, wenn es
/// annehmbar ist (RAD_ContextMenuViewDeployRejection), sonst rot.
///
#define RAD_CONTEXT_MENU_VIEW_DEPLOY_ZONE_COLOR ((SDL_Color){ 40, 200, 80, 70 })
#define RAD_CONTEXT_MENU_VIEW_DEPLOY_TARGET_COLOR ((SDL_Color){ 40, 200, 80, 190 })
#define RAD_CONTEXT_MENU_VIEW_DEPLOY_OUTSIDE_TARGET_COLOR ((SDL_Color){ 220, 40, 40, 190 })

///
/// Farbe der Felder, die die ziehende Einheit vom Ende ihres Weges aus noch
/// erreicht (RAD_ContextMenuViewDrawReach): durchscheinend blau.
///
#define RAD_CONTEXT_MENU_VIEW_REACH_COLOR ((SDL_Color){ 60, 140, 255, 70 })

///
/// Wie weit RAD_ContextMenuViewDrawReach hoechstens sucht, in Feldern um das
/// Ende des Weges: mehr Schritte als RAD_NET_PATH_MAX_STEPS - 1 hat kein Weg,
/// und jeder kostet wenigstens 1 (RAD_ControlTerrainMovementCost).
///
#define RAD_CONTEXT_MENU_VIEW_REACH_RADIUS (RAD_NET_PATH_MAX_STEPS - 1)
#define RAD_CONTEXT_MENU_VIEW_REACH_SIDE (2 * RAD_CONTEXT_MENU_VIEW_REACH_RADIUS + 1)


///
/// Wie weit eine Waffe der angreifenden Einheit reicht, in Feldern
/// (RAD_ClientWeapon_t): von "min_range" -- 0 heisst ohne Mindestweite -- bis
/// "max_range".
///
typedef struct
{
    int32_t min_range;
    int32_t max_range;
} RAD_ContextMenuViewWeaponRange_t;

///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_ContextMenuView
{
    int32_t center_x;
    int32_t center_y;
    ///
    /// Der Ring liegt mittig auf "radius": innen "radius - ring_width / 2",
    /// aussen "radius + ring_width / 2".
    ///
    int32_t radius;
    int32_t ring_width;
    SDL_Color color;

    ///
    /// Die Eintraege in der Reihenfolge, in der sie hinzugefuegt wurden -- und
    /// gezeichnet werden: "count" belegt von "capacity" Plaetzen.
    ///
    RAD_ContextMenuItem_t **items;
    int32_t count;
    int32_t capacity;

    ///
    /// Ihre UnitDeploymentView (RAD_CONTEXT_MENU_ITEM_ACTION_DEPLOY). Gehoert
    /// ihr; die Reserve darin gehoert der Welt (RAD_ContextMenuViewSetReserve).
    ///
    RAD_UnitDeploymentView_t *unit_deployment_view;

    ///
    /// Wo die UnitDeploymentView sich oeffnet, wenn der Zustand nach
    /// SELECT_ENTITY_FROM_RESERVE wechselt: wo Deploy angeklickt wurde
    /// (RAD_ContextMenuViewRunAction).
    ///
    int32_t deployment_x;
    int32_t deployment_y;

    ///
    /// Ihre Zustandsmaschine (view/context_menu/context_menu_state_machine.h).
    /// Gehoert ihr; aus ihrem Zustand folgt, was zu sehen ist
    /// (RAD_ContextMenuViewSyncState).
    ///
    RAD_ContextMenuStateMachine_t *state_machine;

    ///
    /// Der Zustand, fuer den die Eintraege gebaut sind
    /// (RAD_ContextMenuViewFillItems). Weicht der Zustand der Zustandsmaschine
    /// davon ab, baut RAD_ContextMenuViewSyncState sie neu.
    ///
    RAD_ContextMenuState_t items_state;

    ///
    /// Was zum Deployen gebraucht wird (RAD_ContextMenuViewRunAction): das
    /// ausgewaehlte Feld (RAD_ContextMenuViewOnTileSelected) und die Einheit,
    /// die in der UnitDeploymentView gewaehlt wurde
    /// (RAD_ContextMenuViewTakeDeployment), RAD_NET_ENTITY_NONE fuer keine.
    ///
    int32_t tile_x;
    int32_t tile_y;
    RAD_NetEntityId_t deploy_unit;

    ///
    /// Was zum Ziehen gebraucht wird (RAD_ContextMenuViewRunAction): die
    /// Einheit, die beim Oeffnen auf dem ausgewaehlten Feld stand,
    /// RAD_NET_ENTITY_NONE fuer keine, und ihr Weg, den der Spieler in
    /// ENTITY_MOVE zusammenklickt (RAD_ContextMenuViewExtendPath), mit dem
    /// ausgewaehlten Feld als steps_to[0].
    ///
    RAD_NetEntityId_t move_unit;
    RAD_NetPath_t move_path;

    ///
    /// Das Budget des Weges: die Bewegungsreichweite der Einheit beim Oeffnen
    /// (RAD_ClientUnit_t.movement), und was die Felder des Weges davon schon
    /// kosten (RAD_ControlTerrainMovementCost) -- ohne das Feld der Einheit.
    ///
    int32_t move_budget;
    int32_t move_cost;

    ///
    /// Was zum Angreifen gebraucht wird: wie weit die Waffen der Einheit
    /// reichen, gelesen beim Oeffnen -- "attack_range_count" belegt --, und
    /// das Zielfeld, das der Spieler in ENTITY_ATTACK gewaehlt hat
    /// (RAD_ContextMenuViewSelectTarget).
    ///
    RAD_ContextMenuViewWeaponRange_t attack_ranges[RAD_CLIENT_UNIT_MEMBERS_MAX * RAD_CLIENT_MEMBER_WEAPONS_MAX];
    int32_t attack_range_count;

    ///
    /// Ob die Einheit in diesem Zug noch ziehen bzw. angreifen darf, gelesen
    /// beim Oeffnen (RAD_ClientUnit_t.deployed, moved, attacked): nicht in
    /// diesem Zug aufgestellt, und nicht schon gezogen bzw. angegriffen. Davon
    /// haengt ab, ob Move und Attack angeboten werden (RAD_ContextMenuViewFillItems).
    ///
    bool may_move;
    bool may_attack;
    int32_t target_x;
    int32_t target_y;

    ///
    /// Die Verbindung zum Backend, ueber die die Kommandos hinausgehen
    /// (control/deploy_unit.h, control/move_unit.h). Gehoert dem Aufrufer.
    ///
    RAD_IoNetSession_t *session;

    ///
    /// Die Welt, aus der sie liest, welche Felder die ziehende Einheit noch
    /// erreicht (RAD_ContextMenuViewDrawReach): bekannt, frei, was sie kosten.
    /// Gehoert dem Aufrufer.
    ///
    const RAD_ClientWorld_t *world;

    ///
    /// Die Karte, auf der sie den Weg markiert
    /// (RAD_UpdateContextMenuViewOverlay); nur gelesen. Gehoert dem
    /// Aufrufer.
    ///
    const RAD_IsoMap_t *map;
};


static RAD_ContextMenuItem_t* RAD_ContextMenuViewAddItem(RAD_ContextMenuView_t *context_menu_view, float angle, RAD_ContextMenuItemAction_t action);
static void RAD_ContextMenuViewClearItems(RAD_ContextMenuView_t *context_menu_view);
static void RAD_ContextMenuViewFillItems(RAD_ContextMenuView_t *context_menu_view, RAD_ContextMenuState_t state);
static float RAD_ContextMenuViewItemStep(const RAD_ContextMenuView_t *context_menu_view);
static void RAD_ContextMenuViewPlaceItem(const RAD_ContextMenuView_t *context_menu_view, RAD_ContextMenuItem_t *item);
static void RAD_ContextMenuViewRunAction(RAD_ContextMenuView_t *context_menu_view, RAD_ContextMenuItemAction_t action, const SDL_MouseButtonEvent *button);
static void RAD_ContextMenuViewTakeDeployment(RAD_ContextMenuView_t *context_menu_view);
static void RAD_ContextMenuViewExtendPath(RAD_ContextMenuView_t *context_menu_view, const RAD_ClientTile_t *tile);
static void RAD_ContextMenuViewReadWeaponRanges(RAD_ContextMenuView_t *context_menu_view, const RAD_ClientUnit_t *unit);
static void RAD_ContextMenuViewSelectTarget(RAD_ContextMenuView_t *context_menu_view, const RAD_ClientTile_t *tile);
static void RAD_ContextMenuViewSyncState(RAD_ContextMenuView_t *context_menu_view);
static void RAD_ContextMenuViewFillRing(SDL_Renderer *renderer, float center_x, float center_y, float inner_radius, float outer_radius, SDL_Color color);
static void RAD_ContextMenuViewFillTile(SDL_Renderer *renderer, float center_x, float center_y, SDL_Color color);
static bool RAD_ContextMenuViewInRange(const RAD_ContextMenuView_t *context_menu_view, int32_t distance);
static void RAD_ContextMenuViewDrawPath(const RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer);
static void RAD_ContextMenuViewDrawReach(const RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer);
static bool RAD_ContextMenuViewPathContains(const RAD_ContextMenuView_t *context_menu_view, int32_t x, int32_t y);
static void RAD_ContextMenuViewDrawAttackRange(const RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer, bool with_target);
static void RAD_ContextMenuViewMarkTile(const RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer, int32_t x, int32_t y, SDL_Color color);
static bool RAD_ContextMenuViewInDeployZone(const RAD_ContextMenuView_t *context_menu_view, int32_t x, int32_t y);
static const char* RAD_ContextMenuViewDeployRejection(const RAD_ContextMenuView_t *context_menu_view, int32_t x, int32_t y);
static void RAD_ContextMenuViewDrawDeployZone(const RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer);


RAD_ContextMenuView_t* RAD_CreateContextMenuView(int32_t center_x, int32_t center_y, int32_t radius,
                                                 TTF_Font *font, int32_t deployment_width, int32_t deployment_height,
                                                 RAD_IoNetSession_t *session, const RAD_IsoMap_t *map,
                                                 const RAD_ClientWorld_t *world)
{
    RAD_ContextMenuView_t *context_menu_view = malloc(sizeof(struct RAD_ContextMenuView));
    if(context_menu_view == NULL)
    {
        return NULL;
    }

    *context_menu_view = (struct RAD_ContextMenuView){
        .center_x = center_x,
        .center_y = center_y,
        .radius = radius,
        .ring_width = RAD_CONTEXT_MENU_VIEW_DEFAULT_RING_WIDTH,
        .color = { 96, 96, 96, 255 },
        .items = NULL,
        .count = 0,
        .capacity = 0,
        .unit_deployment_view = NULL,
        .deployment_x = 0,
        .deployment_y = 0,
        .state_machine = NULL,
        .items_state = RAD_CONTEXT_MENU_STATE_IDLE,
        .tile_x = 0,
        .tile_y = 0,
        .deploy_unit = RAD_NET_ENTITY_NONE,
        .move_unit = RAD_NET_ENTITY_NONE,
        .move_path = { .number_of_steps = 0 },
        .move_budget = 0,
        .move_cost = 0,
        .attack_range_count = 0,
        .may_move = false,
        .may_attack = false,
        .target_x = 0,
        .target_y = 0,
        .session = session,
        .world = world,
        .map = map
    };

    // Wo sie steht, setzt RAD_ContextMenuViewRunAction beim Oeffnen.
    context_menu_view->unit_deployment_view = RAD_CreateUnitDeploymentView(0, 0, deployment_width, deployment_height, font);
    context_menu_view->state_machine = RAD_CreateContextMenuStateMachine();
    if(context_menu_view->unit_deployment_view == NULL || context_menu_view->state_machine == NULL)
    {
        RAD_DestroyContextMenuView(&context_menu_view);
        return NULL;
    }

    return context_menu_view;
}

void RAD_DestroyContextMenuView(RAD_ContextMenuView_t **context_menu_view)
{
    RAD_ContextMenuView_t *v = *context_menu_view;

    for(int32_t i = 0; i < v->count; i++)
    {
        RAD_DestroyContextMenuItem(&v->items[i]);
    }
    free(v->items);

    // Auch eine halb angelegte (RAD_CreateContextMenuView im Fehlerfall).
    if(v->unit_deployment_view != NULL)
    {
        RAD_DestroyUnitDeploymentView(&v->unit_deployment_view);
    }
    if(v->state_machine != NULL)
    {
        RAD_DestroyContextMenuStateMachine(&v->state_machine);
    }

    free(v);
    *context_menu_view = NULL;
}

void RAD_ContextMenuViewSetCenter(RAD_ContextMenuView_t *context_menu_view, int32_t center_x, int32_t center_y)
{
    context_menu_view->center_x = center_x;
    context_menu_view->center_y = center_y;

    for(int32_t i = 0; i < context_menu_view->count; i++)
    {
        RAD_ContextMenuViewPlaceItem(context_menu_view, context_menu_view->items[i]);
    }
}

void RAD_ContextMenuViewOnTileSelected(RAD_ContextMenuView_t *context_menu_view, const RAD_ClientTile_t *tile)
{
    // Ist das Spiel vorbei, gibt es nichts mehr zu tun: kein Menue, und ein
    // offenes schliesst sich.
    if(context_menu_view->world != NULL && RAD_ClientWorldIsFinished(context_menu_view->world))
    {
        RAD_ContextMenuStateMachineOnCancel(context_menu_view->state_machine);
        RAD_ContextMenuViewSyncState(context_menu_view);
        return;
    }

    // Wird ein Weg zusammengeklickt, ist jedes Feld ein Schritt darauf; wird
    // ein Ziel gesucht, ist es das Ziel.
    const RAD_ContextMenuState_t state = RAD_ContextMenuStateMachineState(context_menu_view->state_machine);
    if(state == RAD_CONTEXT_MENU_STATE_ENTITY_MOVE)
    {
        // Das Feld der Einheit bricht den Weg ab; das Menue kommt dort wieder.
        if(tile != NULL && tile->x == context_menu_view->tile_x && tile->y == context_menu_view->tile_y)
        {
            printf("[ContextMenuView] Weg abgebrochen auf (%d, %d)\n", (int)tile->x, (int)tile->y);
            context_menu_view->move_path.number_of_steps = 0;
            context_menu_view->move_cost = 0;
            RAD_ContextMenuStateMachineOnMoveAborted(context_menu_view->state_machine);
        }
        else
        {
            RAD_ContextMenuViewExtendPath(context_menu_view, tile);
        }
        RAD_ContextMenuViewSyncState(context_menu_view);
        return;
    }
    if(state == RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK)
    {
        // Das Feld der Einheit bricht die Zielwahl ab; das Menue kommt dort
        // wieder.
        if(tile != NULL && tile->x == context_menu_view->tile_x && tile->y == context_menu_view->tile_y)
        {
            printf("[ContextMenuView] Zielwahl abgebrochen auf (%d, %d)\n", (int)tile->x, (int)tile->y);
            RAD_ContextMenuStateMachineOnAttackAborted(context_menu_view->state_machine);
        }
        else
        {
            RAD_ContextMenuViewSelectTarget(context_menu_view, tile);
        }
        RAD_ContextMenuViewSyncState(context_menu_view);
        return;
    }

    const bool was_active = RAD_ContextMenuViewIsActive(context_menu_view);
    // Der Spieler ist der dieses Clients (io/net_session.h).
    RAD_ContextMenuStateMachineOnTileSelected(context_menu_view->state_machine, tile,
                                              (RAD_ClientPlayerId_t)context_menu_view->session->own_player_id);

    // Oeffnet sie sich damit, ist "tile" das Feld, auf das deployt wuerde
    // bzw. von dem aus die Einheit darauf zieht.
    if(!was_active && RAD_ContextMenuViewIsActive(context_menu_view) && tile != NULL)
    {
        context_menu_view->tile_x = tile->x;
        context_menu_view->tile_y = tile->y;
        context_menu_view->deploy_unit = RAD_NET_ENTITY_NONE;
        context_menu_view->move_unit = (tile->unit != NULL) ? (RAD_NetEntityId_t)tile->unit->id : RAD_NET_ENTITY_NONE;
        context_menu_view->move_path.number_of_steps = 0;
        context_menu_view->move_budget = (tile->unit != NULL) ? (int32_t)tile->unit->movement : 0;
        context_menu_view->move_cost = 0;
        RAD_ContextMenuViewReadWeaponRanges(context_menu_view, tile->unit);
        context_menu_view->may_move = (tile->unit != NULL) && !tile->unit->deployed && !tile->unit->moved;
        context_menu_view->may_attack = (tile->unit != NULL) && !tile->unit->deployed && !tile->unit->attacked;
    }
    RAD_ContextMenuViewSyncState(context_menu_view);
}

bool RAD_ContextMenuViewIsActive(const RAD_ContextMenuView_t *context_menu_view)
{
    return RAD_ContextMenuStateMachineState(context_menu_view->state_machine) != RAD_CONTEXT_MENU_STATE_IDLE;
}

bool RAD_ContextMenuViewIsVisible(const RAD_ContextMenuView_t *context_menu_view)
{
    // Waehrend der Weg zusammengeklickt oder das Ziel gesucht wird, ist sie
    // aus dem Weg.
    const RAD_ContextMenuState_t state = RAD_ContextMenuStateMachineState(context_menu_view->state_machine);
    return state != RAD_CONTEXT_MENU_STATE_IDLE
        && state != RAD_CONTEXT_MENU_STATE_ENTITY_MOVE
        && state != RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK;
}

bool RAD_ContextMenuViewSelectedTile(const RAD_ContextMenuView_t *context_menu_view, int32_t *x, int32_t *y)
{
    if(!RAD_ContextMenuViewIsActive(context_menu_view))
    {
        return false;
    }

    *x = context_menu_view->tile_x;
    *y = context_menu_view->tile_y;
    return true;
}

bool RAD_ContextMenuViewAnchorTile(const RAD_ContextMenuView_t *context_menu_view, int32_t *x, int32_t *y)
{
    if(!RAD_ContextMenuViewIsVisible(context_menu_view))
    {
        return false;
    }

    // Steht der Weg, oeffnet sie sich ueber seinem Ziel; steht das Ziel des
    // Angriffs, ueber diesem.
    const RAD_ContextMenuState_t state = RAD_ContextMenuStateMachineState(context_menu_view->state_machine);
    const RAD_NetPath_t *path = &context_menu_view->move_path;
    if(state == RAD_CONTEXT_MENU_STATE_WAITING_FOR_MOVE_ACKNOWLEDGEMENT && path->number_of_steps > 0)
    {
        *x = path->steps_to[path->number_of_steps - 1].x;
        *y = path->steps_to[path->number_of_steps - 1].y;
        return true;
    }
    if(state == RAD_CONTEXT_MENU_STATE_WAITING_FOR_ATTACK_ACKNOWLEDGEMENT)
    {
        *x = context_menu_view->target_x;
        *y = context_menu_view->target_y;
        return true;
    }

    *x = context_menu_view->tile_x;
    *y = context_menu_view->tile_y;
    return true;
}

void RAD_ContextMenuViewSetReserve(RAD_ContextMenuView_t *context_menu_view,
                                   const RAD_ClientReserve_t *reserve,
                                   const RAD_ClientUnitRepository_t *units)
{
    RAD_UnitDeploymentViewSetReserve(context_menu_view->unit_deployment_view, reserve, units);
}

void RAD_ContextMenuViewSetRingWidth(RAD_ContextMenuView_t *context_menu_view, int32_t ring_width)
{
    context_menu_view->ring_width = (ring_width > 0) ? ring_width : 0;
}

void RAD_ContextMenuViewSetColor(RAD_ContextMenuView_t *context_menu_view, SDL_Color color)
{
    context_menu_view->color = color;
}

int32_t RAD_ContextMenuViewItemCount(const RAD_ContextMenuView_t *context_menu_view)
{
    return context_menu_view->count;
}

bool RAD_ContextMenuViewHandleMouseMotion(RAD_ContextMenuView_t *context_menu_view, const SDL_MouseMotionEvent *motion)
{
    if(!RAD_ContextMenuViewIsVisible(context_menu_view))
    {
        return false;
    }

    // Die UnitDeploymentView liegt ueber dem Ring: was sie nimmt, sehen die
    // Eintraege nicht.
    if(RAD_UnitDeploymentViewHandleMouseMotion(context_menu_view->unit_deployment_view, motion))
    {
        return true;
    }

    for(int32_t i = 0; i < context_menu_view->count; i++)
    {
        RAD_ContextMenuItemHandleMouseMotion(context_menu_view->items[i], motion);
    }
    return false;
}

bool RAD_ContextMenuViewHandleMouseButton(RAD_ContextMenuView_t *context_menu_view, const SDL_MouseButtonEvent *button)
{
    if(!RAD_ContextMenuViewIsVisible(context_menu_view))
    {
        return false;
    }

    // Zuerst die UnitDeploymentView, sie liegt ueber dem Ring.
    if(RAD_UnitDeploymentViewHandleMouseButton(context_menu_view->unit_deployment_view, button))
    {
        RAD_ContextMenuViewTakeDeployment(context_menu_view);
        return true;
    }

    // Von oben nach unten: der zuletzt hinzugefuegte liegt oben und bekommt
    // es zuerst. Wer es annimmt, ist der einzige.
    for(int32_t i = context_menu_view->count - 1; i >= 0; i--)
    {
        RAD_ContextMenuItem_t *item = context_menu_view->items[i];
        if(!RAD_ContextMenuItemHandleMouseButton(item, button))
        {
            continue;
        }

        // Ein Klick: die linke Maustaste ueber dem Eintrag losgelassen.
        if(button->type == SDL_MOUSEBUTTONUP && button->button == SDL_BUTTON_LEFT)
        {
            RAD_ContextMenuViewRunAction(context_menu_view, RAD_ContextMenuItemAction(item), button);
        }
        return true;
    }
    return false;
}

void RAD_UpdateContextMenuView(RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer)
{
    // Was zu sehen ist, folgt aus dem Zustand: in IDLE nichts, sonst Ring und
    // die Eintraege des Zustands, in SELECT_ENTITY_FROM_RESERVE dazu die
    // UnitDeploymentView (RAD_ContextMenuViewSyncState).
    if(!RAD_ContextMenuViewIsVisible(context_menu_view))
    {
        return;
    }

    const float center_x = (float)context_menu_view->center_x;
    const float center_y = (float)context_menu_view->center_y;
    const float radius = (float)context_menu_view->radius;

    // Mit Alpha gemischt, damit eine durchscheinende Farbe auch durchscheint;
    // ohne Textur nimmt SDL_RenderGeometry den Blend-Modus des Renderers.
    // Danach den des Aufrufers wiederherstellen.
    SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &previous_blend_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    // Innen nicht unter den Mittelpunkt: ein breiterer Ring wird zur Scheibe.
    const float half_width = 0.5f * (float)context_menu_view->ring_width;
    const float inner_radius = (radius > half_width) ? radius - half_width : 0.0f;
    RAD_ContextMenuViewFillRing(renderer, center_x, center_y, inner_radius, radius + half_width, context_menu_view->color);

    // Welche es sind und ob einer hervorgehoben ist, folgt aus dem Zustand
    // (RAD_ContextMenuViewFillItems).
    for(int32_t i = 0; i < context_menu_view->count; i++)
    {
        RAD_UpdateContextMenuItem(context_menu_view->items[i], renderer);
    }

    SDL_SetRenderDrawBlendMode(renderer, previous_blend_mode);

    // Ueber allem, auch ueber dem Ring; zeichnet nur, wenn sie offen ist.
    RAD_UpdateUnitDeploymentView(context_menu_view->unit_deployment_view, renderer);
}

void RAD_UpdateContextMenuViewOverlay(RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer)
{
    // Was markiert wird, folgt aus dem Zustand; die ContextMenuView selbst ist
    // dabei mal zu sehen, mal nicht.
    const RAD_ContextMenuState_t state = RAD_ContextMenuStateMachineState(context_menu_view->state_machine);
    const bool path = (state == RAD_CONTEXT_MENU_STATE_ENTITY_MOVE)
                   || (state == RAD_CONTEXT_MENU_STATE_WAITING_FOR_MOVE_ACKNOWLEDGEMENT);
    const bool attack = (state == RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK)
                     || (state == RAD_CONTEXT_MENU_STATE_WAITING_FOR_ATTACK_ACKNOWLEDGEMENT);
    // Ab dem Klick auf Deploy, bis es bestaetigt oder verworfen ist.
    const bool deploy = (state == RAD_CONTEXT_MENU_STATE_SELECT_ENTITY_FROM_RESERVE)
                     || (state == RAD_CONTEXT_MENU_STATE_DEPLOY_FROM_RESERVE)
                     || (state == RAD_CONTEXT_MENU_STATE_WAITING_FOR_USER_ACKNOWLEDGEMENT);
    if(!path && !attack && !deploy)
    {
        return;
    }

    // Mit Alpha gemischt wie der Ring; danach den Blend-Modus des Aufrufers
    // wiederherstellen.
    SDL_BlendMode previous_blend_mode = SDL_BLENDMODE_NONE;
    SDL_GetRenderDrawBlendMode(renderer, &previous_blend_mode);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);

    if(path)
    {
        // Solange der Weg zusammengeklickt wird, liegt darunter, wohin er noch
        // fuehren kann.
        if(state == RAD_CONTEXT_MENU_STATE_ENTITY_MOVE)
        {
            RAD_ContextMenuViewDrawReach(context_menu_view, renderer);
        }
        RAD_ContextMenuViewDrawPath(context_menu_view, renderer);
    }
    else if(attack)
    {
        RAD_ContextMenuViewDrawAttackRange(context_menu_view, renderer,
                                           state == RAD_CONTEXT_MENU_STATE_WAITING_FOR_ATTACK_ACKNOWLEDGEMENT);
    }
    else
    {
        RAD_ContextMenuViewDrawDeployZone(context_menu_view, renderer);
    }

    SDL_SetRenderDrawBlendMode(renderer, previous_blend_mode);
}


///
/// Legt einen Eintrag am Winkel "angle" mit der Aktion "action" an, setzt ihn auf den Ring und haengt ihn hinten an; NULL, wenn
/// kein Speicher da ist -- dann fehlt nur dieser Eintrag.
///
static RAD_ContextMenuItem_t* RAD_ContextMenuViewAddItem(RAD_ContextMenuView_t *context_menu_view, float angle, RAD_ContextMenuItemAction_t action)
{
    if(context_menu_view->count == context_menu_view->capacity)
    {
        const int32_t capacity = (context_menu_view->capacity > 0)
            ? 2 * context_menu_view->capacity
            : RAD_CONTEXT_MENU_VIEW_INITIAL_CAPACITY;
        RAD_ContextMenuItem_t **items = realloc(context_menu_view->items, (size_t)capacity * sizeof(RAD_ContextMenuItem_t*));
        if(items == NULL)
        {
            return NULL;
        }
        context_menu_view->items = items;
        context_menu_view->capacity = capacity;
    }

    RAD_ContextMenuItem_t *item = RAD_CreateContextMenuItem(angle, RAD_CONTEXT_MENU_VIEW_ITEM_RADIUS);
    if(item == NULL)
    {
        return NULL;
    }
    RAD_ContextMenuItemSetAction(item, action);
    RAD_ContextMenuViewPlaceItem(context_menu_view, item);

    context_menu_view->items[context_menu_view->count] = item;
    context_menu_view->count++;
    return item;
}

///
/// Entfernt alle Eintraege. Zeiger darauf gelten danach nicht mehr.
///
static void RAD_ContextMenuViewClearItems(RAD_ContextMenuView_t *context_menu_view)
{
    for(int32_t i = 0; i < context_menu_view->count; i++)
    {
        RAD_DestroyContextMenuItem(&context_menu_view->items[i]);
    }
    // Die Plaetze bleiben fuer die naechsten Eintraege.
    context_menu_view->count = 0;
}

///
/// Baut die Eintraege fuer den Zustand "state" neu; was vorher stand, faellt
/// weg. Ausser in IDLE, ENTITY_MOVE und ENTITY_ATTACK steht immer unten links
/// Schliessen
/// (RAD_CONTEXT_MENU_ITEM_ACTION_CLOSE), rechts davon, oben auf dem Scheitel
/// beginnend:
///
///   - IDLE, ENTITY_MOVE, ENTITY_ATTACK: keine Eintraege -- sie ist
///     unsichtbar.
///   - EMPTY_TILE_SELECTED, SELECT_ENTITY_FROM_RESERVE, DEPLOY_FROM_RESERVE:
///     Deploy (RAD_CONTEXT_MENU_ITEM_ACTION_DEPLOY).
///   - WAITING_FOR_USER_ACKNOWLEDGEMENT: Deploy, blau hervorgehoben -- wenn
///     das ausgewaehlte Feld in der eigenen Aufstellzone liegt, sonst nicht:
///     dann nimmt Deploy es auch nicht an (RAD_ContextMenuViewRunAction).
///   - TILE_WITH_OWN_ENTITY_SELECTED: Move (RAD_CONTEXT_MENU_ITEM_ACTION_MOVE),
///     wenn die Einheit noch ziehen darf, und Attack
///     (RAD_CONTEXT_MENU_ITEM_ACTION_ATTACK), wenn sie noch angreifen darf
///     ("may_move", "may_attack"). Jeder an seinem festen Platz, auch wenn der
///     andere fehlt.
///   - WAITING_FOR_MOVE_ACKNOWLEDGEMENT: Move, blau hervorgehoben.
///   - WAITING_FOR_ATTACK_ACKNOWLEDGEMENT: Attack, blau hervorgehoben.
///   - TILE_WITH_FOREIGN_ENTITY_SELECTED, WAITING_FOR_SERVER_RESPONSE: nur
///     Schliessen.
///
static void RAD_ContextMenuViewFillItems(RAD_ContextMenuView_t *context_menu_view, RAD_ContextMenuState_t state)
{
    RAD_ContextMenuViewClearItems(context_menu_view);
    context_menu_view->items_state = state;
    // Unsichtbar: in IDLE und waehrend der Weg zusammengeklickt oder das Ziel
    // gesucht wird.
    if(state == RAD_CONTEXT_MENU_STATE_IDLE
       || state == RAD_CONTEXT_MENU_STATE_ENTITY_MOVE
       || state == RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK)
    {
        return;
    }

    const float first_right = RAD_CONTEXT_MENU_VIEW_FIRST_RIGHT_ANGLE;
    RAD_ContextMenuViewAddItem(context_menu_view, RAD_CONTEXT_MENU_VIEW_CLOSE_ANGLE, RAD_CONTEXT_MENU_ITEM_ACTION_CLOSE);

    switch(state)
    {
        case RAD_CONTEXT_MENU_STATE_EMPTY_TILE_SELECTED:
        case RAD_CONTEXT_MENU_STATE_SELECT_ENTITY_FROM_RESERVE:
        case RAD_CONTEXT_MENU_STATE_WAITING_FOR_USER_ACKNOWLEDGEMENT:
        case RAD_CONTEXT_MENU_STATE_DEPLOY_FROM_RESERVE:
        {
            RAD_ContextMenuItem_t *deploy = RAD_ContextMenuViewAddItem(context_menu_view, first_right, RAD_CONTEXT_MENU_ITEM_ACTION_DEPLOY);
            // Wartet sie darauf, dass der Spieler das Deployen bestaetigt, ist
            // Deploy hervorgehoben -- blau, aber nur, wenn es auch hinausgeht.
            if(deploy != NULL && state == RAD_CONTEXT_MENU_STATE_WAITING_FOR_USER_ACKNOWLEDGEMENT
               && RAD_ContextMenuViewDeployRejection(context_menu_view, context_menu_view->tile_x, context_menu_view->tile_y) == NULL)
            {
                RAD_ContextMenuItemSetHighlighted(deploy, true);
            }
            break;
        }
        case RAD_CONTEXT_MENU_STATE_TILE_WITH_OWN_ENTITY_SELECTED:
            // Was die Einheit in diesem Zug nicht mehr darf, wird nicht angeboten.
            if(context_menu_view->may_move)
            {
                RAD_ContextMenuViewAddItem(context_menu_view, first_right, RAD_CONTEXT_MENU_ITEM_ACTION_MOVE);
            }
            if(context_menu_view->may_attack)
            {
                RAD_ContextMenuViewAddItem(context_menu_view, first_right + RAD_ContextMenuViewItemStep(context_menu_view), RAD_CONTEXT_MENU_ITEM_ACTION_ATTACK);
            }
            break;
        case RAD_CONTEXT_MENU_STATE_WAITING_FOR_ATTACK_ACKNOWLEDGEMENT:
        {
            // Das Ziel steht: Attack bestaetigt es, hervorgehoben -- blau.
            RAD_ContextMenuItem_t *attack = RAD_ContextMenuViewAddItem(context_menu_view, first_right, RAD_CONTEXT_MENU_ITEM_ACTION_ATTACK);
            if(attack != NULL)
            {
                RAD_ContextMenuItemSetHighlighted(attack, true);
            }
            break;
        }
        case RAD_CONTEXT_MENU_STATE_WAITING_FOR_MOVE_ACKNOWLEDGEMENT:
        {
            // Der Weg steht: Move bestaetigt ihn, hervorgehoben -- blau.
            RAD_ContextMenuItem_t *move = RAD_ContextMenuViewAddItem(context_menu_view, first_right, RAD_CONTEXT_MENU_ITEM_ACTION_MOVE);
            if(move != NULL)
            {
                RAD_ContextMenuItemSetHighlighted(move, true);
            }
            break;
        }
        // Eine fremde Einheit befehligt der Spieler nicht.
        case RAD_CONTEXT_MENU_STATE_TILE_WITH_FOREIGN_ENTITY_SELECTED:
        case RAD_CONTEXT_MENU_STATE_WAITING_FOR_SERVER_RESPONSE:
        case RAD_CONTEXT_MENU_STATE_IDLE:
        case RAD_CONTEXT_MENU_STATE_ENTITY_MOVE:
        case RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK:
            break;
    }
}

///
/// Um wie viel Grad zwei benachbarte Eintraege auf dem Ring auseinander
/// stehen: so weit, dass ihre Mittelpunkte -- beide auf dem Radius -- zwei
/// Eintragsradien plus RAD_CONTEXT_MENU_VIEW_ITEM_GAP voneinander entfernt
/// sind. Bei 200 Pixeln Radius sind das gut 57 Grad. Ist der Ring dafuer zu klein,
/// 180 Grad: gegenueber.
///
static float RAD_ContextMenuViewItemStep(const RAD_ContextMenuView_t *context_menu_view)
{
    // Die Sehne zwischen zwei Punkten auf dem Kreis ist 2 r sin(Winkel / 2).
    const float distance = (float)(2 * RAD_CONTEXT_MENU_VIEW_ITEM_RADIUS + RAD_CONTEXT_MENU_VIEW_ITEM_GAP);
    const float diameter = 2.0f * (float)context_menu_view->radius;
    if(diameter <= distance)
    {
        return 180.0f;
    }
    return 2.0f * asinf(distance / diameter) * 180.0f / (float)M_PI;
}

///
/// Setzt den Mittelpunkt von "item" (view/context_menu/context_menu_item.h): auf den Radius,
/// an seinen Winkel. Nach jeder Aenderung des Mittelpunkts der ContextMenuView
/// und fuer jeden neuen Eintrag zu rufen.
///
static void RAD_ContextMenuViewPlaceItem(const RAD_ContextMenuView_t *context_menu_view, RAD_ContextMenuItem_t *item)
{
    // 0 Grad ist oben, also -90 Grad fuer cosf/sinf. Die y-Achse zeigt nach
    // unten, ein wachsender Winkel dreht also im Uhrzeigersinn.
    const float angle = (RAD_ContextMenuItemAngle(item) - 90.0f) * (float)M_PI / 180.0f;
    RAD_ContextMenuItemSetCenter(
        item,
        (float)context_menu_view->center_x + (float)context_menu_view->radius * cosf(angle),
        (float)context_menu_view->center_y + (float)context_menu_view->radius * sinf(angle)
    );
}

///
/// Fuehrt die Aktion eines angeklickten Eintrags aus
/// (view/context_menu/context_menu_item.h); "button" ist der Klick.
///
static void RAD_ContextMenuViewRunAction(RAD_ContextMenuView_t *context_menu_view, RAD_ContextMenuItemAction_t action, const SDL_MouseButtonEvent *button)
{
    // War das Menue noch offen, als das Spiel endete, schliesst jeder Klick es nur
    // noch -- hinaus geht nichts mehr.
    if(context_menu_view->world != NULL && RAD_ClientWorldIsFinished(context_menu_view->world))
    {
        RAD_ContextMenuStateMachineOnCancel(context_menu_view->state_machine);
        return;
    }

    switch(action)
    {
        case RAD_CONTEXT_MENU_ITEM_ACTION_CLOSE:
            RAD_ContextMenuStateMachineOnCancel(context_menu_view->state_machine);
            break;
        case RAD_CONTEXT_MENU_ITEM_ACTION_DEPLOY:
        {
            // Oeffnet sich die UnitDeploymentView, dann mit ihrer oberen
            // linken Ecke unter der Maus.
            context_menu_view->deployment_x = button->x;
            context_menu_view->deployment_y = button->y;

            // Wartet sie auf die Bestaetigung des Spielers, ist dieser Klick
            // sie: dann geht das Kommando hinaus (control/deploy_unit.h) --
            // aber nur auf ein Feld, das der Server annehmen wuerde, wie
            // markiert (RAD_ContextMenuViewDeployRejection). Sonst nur ins
            // Log; der Server prueft ohnehin selbst.
            const bool confirmed = RAD_ContextMenuStateMachineState(context_menu_view->state_machine)
                == RAD_CONTEXT_MENU_STATE_WAITING_FOR_USER_ACKNOWLEDGEMENT;
            RAD_ContextMenuStateMachineOnDeploymentRequested(context_menu_view->state_machine);
            const char *rejected = RAD_ContextMenuViewDeployRejection(context_menu_view, context_menu_view->tile_x, context_menu_view->tile_y);
            if(confirmed && rejected != NULL)
            {
                printf("[ContextMenuView] Deploy auf (%d, %d) nicht geschickt: %s\n",
                       (int)context_menu_view->tile_x, (int)context_menu_view->tile_y, rejected);
            }
            if(confirmed && rejected == NULL && context_menu_view->deploy_unit != RAD_NET_ENTITY_NONE)
            {
                RAD_ControlDeployUnit(context_menu_view->session, context_menu_view->deploy_unit,
                                      (int16_t)context_menu_view->tile_x, (int16_t)context_menu_view->tile_y);
            }
            break;
        }
        case RAD_CONTEXT_MENU_ITEM_ACTION_MOVE:
        {
            printf("[ContextMenuView] Move angeklickt fuer Feld (%d, %d)\n",
                   (int)context_menu_view->tile_x, (int)context_menu_view->tile_y);

            // Wartet sie auf die Bestaetigung des Weges, ist dieser Klick sie:
            // dann geht das Kommando hinaus (control/move_unit.h).
            const RAD_ContextMenuState_t before = RAD_ContextMenuStateMachineState(context_menu_view->state_machine);
            RAD_ContextMenuStateMachineOnMoveRequested(context_menu_view->state_machine);
            const RAD_ContextMenuState_t after = RAD_ContextMenuStateMachineState(context_menu_view->state_machine);

            RAD_NetPath_t *path = &context_menu_view->move_path;
            if(after == RAD_CONTEXT_MENU_STATE_ENTITY_MOVE && before != RAD_CONTEXT_MENU_STATE_ENTITY_MOVE)
            {
                // Der Weg beginnt, wo die Einheit steht.
                path->steps_to[0] = (RAD_NetPosition_t){ .x = (int16_t)context_menu_view->tile_x, .y = (int16_t)context_menu_view->tile_y };
                path->number_of_steps = 1;
                context_menu_view->move_cost = 0;
                printf("[ContextMenuView] Weg beginnt bei (%d, %d), Reichweite %d\n",
                       (int)context_menu_view->tile_x, (int)context_menu_view->tile_y, (int)context_menu_view->move_budget);
            }
            else if(before == RAD_CONTEXT_MENU_STATE_WAITING_FOR_MOVE_ACKNOWLEDGEMENT)
            {
                if(path->number_of_steps >= 2 && context_menu_view->move_unit != RAD_NET_ENTITY_NONE)
                {
                    printf("[ContextMenuView] Einheit %d zieht nach (%d, %d), %d Schritte\n",
                           (int)context_menu_view->move_unit,
                           (int)path->steps_to[path->number_of_steps - 1].x, (int)path->steps_to[path->number_of_steps - 1].y,
                           (int)path->number_of_steps - 1);
                    RAD_ControlMoveUnit(context_menu_view->session, context_menu_view->move_unit, path);
                }
                else
                {
                    printf("[ContextMenuView] Weg ohne Schritt, nichts geschickt\n");
                }
                path->number_of_steps = 0;
            }
            break;
        }
        case RAD_CONTEXT_MENU_ITEM_ACTION_ATTACK:
        {
            printf("[ContextMenuView] Attack angeklickt fuer Feld (%d, %d)\n",
                   (int)context_menu_view->tile_x, (int)context_menu_view->tile_y);

            // Wartet sie auf die Bestaetigung des Ziels, ist dieser Klick sie:
            // dann geht das Kommando hinaus (control/attack_unit.h).
            const bool confirmed = RAD_ContextMenuStateMachineState(context_menu_view->state_machine)
                == RAD_CONTEXT_MENU_STATE_WAITING_FOR_ATTACK_ACKNOWLEDGEMENT;
            RAD_ContextMenuStateMachineOnAttackRequested(context_menu_view->state_machine);
            if(confirmed && context_menu_view->move_unit != RAD_NET_ENTITY_NONE)
            {
                printf("[ContextMenuView] Einheit %d greift Feld (%d, %d) an\n",
                       (int)context_menu_view->move_unit, (int)context_menu_view->target_x, (int)context_menu_view->target_y);
                RAD_ControlAttackUnit(context_menu_view->session, context_menu_view->move_unit,
                                      (int16_t)context_menu_view->target_x, (int16_t)context_menu_view->target_y);
            }
            else if(RAD_ContextMenuStateMachineState(context_menu_view->state_machine) == RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK)
            {
                printf("[ContextMenuView] Ziel gesucht fuer Einheit auf (%d, %d), %d Waffen\n",
                       (int)context_menu_view->tile_x, (int)context_menu_view->tile_y, (int)context_menu_view->attack_range_count);
            }
            break;
        }
        case RAD_CONTEXT_MENU_ITEM_ACTION_NONE:
        default:
            break;
    }
    RAD_ContextMenuViewSyncState(context_menu_view);
}

///
/// Nach jedem Maustasten-Ereignis, das die UnitDeploymentView genommen hat:
/// hat sie damit eine Einheit ausgewaehlt, geht die ins Log, und die
/// Zustandsmaschine erfaehrt es (OnEntityFromReserveSelected) -- danach
/// wartet sie darauf, dass der Spieler noch einmal Deploy anklickt, die
/// UnitDeploymentView schliesst sich.
///
static void RAD_ContextMenuViewTakeDeployment(RAD_ContextMenuView_t *context_menu_view)
{
    const RAD_ClientUnit_t *unit = RAD_UnitDeploymentViewSelectedUnit(context_menu_view->unit_deployment_view);
    if(unit == NULL)
    {
        return;
    }

    printf("[ContextMenuView] Einheit zum Deployen ausgewaehlt: %s (Id %d)\n", unit->name, (int)unit->id);
    context_menu_view->deploy_unit = unit->id;
    RAD_UnitDeploymentViewClearSelection(context_menu_view->unit_deployment_view);
    RAD_ContextMenuStateMachineOnEntityFromReserveSelected(context_menu_view->state_machine);
    RAD_ContextMenuViewSyncState(context_menu_view);
}

///
/// In ENTITY_MOVE: wertet aus, dass der Spieler das Feld "tile" angeklickt
/// hat, NULL fuer keins. Jedes Feld geht mit dem, was daraus folgt, ins Log:
///
///   - das letzte Feld des Weges noch einmal, sobald er wenigstens einen
///     Schritt hat: der Weg steht (OnPathCompleted). Das Feld der Einheit
///     kommt hier nicht an, es bricht den Weg ab
///     (RAD_ContextMenuViewOnTileSelected).
///   - ein waagerecht oder senkrecht angrenzendes, freies und begehbares Feld
///     (RAD_ControlTerrainImpassable), das noch nicht im Weg ist, solange der Weg kuerzer als RAD_NET_PATH_MAX_STEPS
///     Felder ist und seine Kosten (RAD_ControlTerrainMovementCost) zusammen
///     mit denen der Felder davor die Reichweite der Einheit nicht
///     uebersteigen: haengt es an.
///   - alles andere, auch keins: nichts.
///
static void RAD_ContextMenuViewExtendPath(RAD_ContextMenuView_t *context_menu_view, const RAD_ClientTile_t *tile)
{
    RAD_NetPath_t *path = &context_menu_view->move_path;
    if(tile == NULL || path->number_of_steps <= 0)
    {
        return;
    }

    const RAD_NetPosition_t last = path->steps_to[path->number_of_steps - 1];
    if(tile->x == last.x && tile->y == last.y)
    {
        printf("[ContextMenuView] Weg steht: %d Schritte bis (%d, %d)\n",
               (int)path->number_of_steps - 1, (int)tile->x, (int)tile->y);
        RAD_ContextMenuStateMachineOnPathCompleted(context_menu_view->state_machine);
        return;
    }

    const bool in_path = RAD_ContextMenuViewPathContains(context_menu_view, tile->x, tile->y);

    const int32_t cost = RAD_ControlTerrainMovementCost(tile);
    const char *rejected = NULL;
    if(in_path)
    {
        rejected = "schon im Weg";
    }
    else if(abs(tile->x - last.x) + abs(tile->y - last.y) != 1)
    {
        rejected = "kein Nachbar des letzten Feldes";
    }
    else if(RAD_ClientTileHasUnit(tile))
    {
        rejected = "besetzt";
    }
    else if(RAD_ControlTerrainImpassable(tile))
    {
        rejected = "unbegehbar";
    }
    else if(path->number_of_steps >= RAD_NET_PATH_MAX_STEPS)
    {
        rejected = "Weg ist voll";
    }
    else if(context_menu_view->move_cost + cost > context_menu_view->move_budget)
    {
        rejected = "Reichweite reicht nicht";
    }

    if(rejected != NULL)
    {
        printf("[ContextMenuView] Feld (%d, %d) nicht in den Weg: %s (kostet %d, verbraucht %d von %d)\n",
               (int)tile->x, (int)tile->y, rejected,
               (int)cost, (int)context_menu_view->move_cost, (int)context_menu_view->move_budget);
        return;
    }

    path->steps_to[path->number_of_steps] = (RAD_NetPosition_t){ .x = (int16_t)tile->x, .y = (int16_t)tile->y };
    path->number_of_steps++;
    context_menu_view->move_cost += cost;
    printf("[ContextMenuView] Feld (%d, %d) in den Weg, Schritt %d, kostet %d, verbraucht %d von %d\n",
           (int)tile->x, (int)tile->y, (int)path->number_of_steps - 1,
           (int)cost, (int)context_menu_view->move_cost, (int)context_menu_view->move_budget);
}

///
/// Liest, wie weit die Waffen aller Mitglieder von "unit" reichen, nach
/// "attack_ranges"; NULL: keine Waffen.
///
static void RAD_ContextMenuViewReadWeaponRanges(RAD_ContextMenuView_t *context_menu_view, const RAD_ClientUnit_t *unit)
{
    context_menu_view->attack_range_count = 0;
    if(unit == NULL)
    {
        return;
    }

    for(uint32_t m = 0; m < unit->number_of_members && m < RAD_CLIENT_UNIT_MEMBERS_MAX; m++)
    {
        const RAD_ClientMember_t *member = &unit->members[m];
        for(uint32_t w = 0; w < member->number_of_weapons && w < RAD_CLIENT_MEMBER_WEAPONS_MAX; w++)
        {
            context_menu_view->attack_ranges[context_menu_view->attack_range_count++] = (RAD_ContextMenuViewWeaponRange_t){
                .min_range = member->weapons[w].min_range,
                .max_range = member->weapons[w].max_range
            };
        }
    }
}

///
/// In ENTITY_ATTACK: wertet aus, dass der Spieler das Feld "tile" als Ziel
/// angeklickt hat, NULL fuer keins -- nicht das der Einheit, das bricht die
/// Zielwahl ab (RAD_ContextMenuViewOnTileSelected). Ein Feld ist ein Ziel, wenn
/// keine eigene Einheit darauf steht und wenigstens eine
/// ihrer Waffen die Entfernung abdeckt -- in Feldern waagerecht plus
/// senkrecht, zwischen min_range und max_range. Ein leeres Feld ist ein
/// gueltiges Ziel. Dann merkt sie es sich, und die Zustandsmaschine erfaehrt
/// es (OnTargetSelected); sonst geschieht nichts. Beides geht ins Log.
///
static void RAD_ContextMenuViewSelectTarget(RAD_ContextMenuView_t *context_menu_view, const RAD_ClientTile_t *tile)
{
    if(tile == NULL)
    {
        return;
    }

    const int32_t distance = abs(tile->x - context_menu_view->tile_x) + abs(tile->y - context_menu_view->tile_y);
    const RAD_ClientPlayerId_t own = (RAD_ClientPlayerId_t)context_menu_view->session->own_player_id;

    const bool in_range = RAD_ContextMenuViewInRange(context_menu_view, distance);

    const char *rejected = NULL;
    if(tile->unit != NULL && own != RAD_CLIENT_PLAYER_NONE && tile->unit->owner == own)
    {
        rejected = "eigene Einheit";
    }
    else if(!in_range)
    {
        rejected = "keine Waffe reicht so weit";
    }

    if(rejected != NULL)
    {
        printf("[ContextMenuView] Feld (%d, %d) kein Ziel: %s (Entfernung %d)\n",
               (int)tile->x, (int)tile->y, rejected, (int)distance);
        return;
    }

    context_menu_view->target_x = tile->x;
    context_menu_view->target_y = tile->y;
    printf("[ContextMenuView] Ziel (%d, %d), Entfernung %d, %s\n",
           (int)tile->x, (int)tile->y, (int)distance, (tile->unit != NULL) ? "Einheit darauf" : "leer");
    RAD_ContextMenuStateMachineOnTargetSelected(context_menu_view->state_machine);
}

///
/// Bringt Eintraege und UnitDeploymentView auf den Stand des Zustands. Nach
/// jedem Ereignis an die Zustandsmaschine zu rufen.
///
/// Die Eintraege werden nur bei einem Zustandswechsel neu gebaut
/// (RAD_ContextMenuViewFillItems). Die UnitDeploymentView ist offen genau in
/// SELECT_ENTITY_FROM_RESERVE; geoeffnet wird sie nur beim Wechsel --
/// einblenden liest die Reserve neu --, und zwar bei (deployment_x,
/// deployment_y).
///
static void RAD_ContextMenuViewSyncState(RAD_ContextMenuView_t *context_menu_view)
{
    const RAD_ContextMenuState_t state = RAD_ContextMenuStateMachineState(context_menu_view->state_machine);
    if(state != context_menu_view->items_state)
    {
        RAD_ContextMenuViewFillItems(context_menu_view, state);
    }

    RAD_UnitDeploymentView_t *deployment = context_menu_view->unit_deployment_view;
    const bool want_open = state == RAD_CONTEXT_MENU_STATE_SELECT_ENTITY_FROM_RESERVE;
    if(want_open == RAD_UnitDeploymentViewIsVisible(deployment))
    {
        return;
    }

    if(want_open)
    {
        RAD_UnitDeploymentViewSetPosition(deployment, context_menu_view->deployment_x, context_menu_view->deployment_y);
    }
    RAD_UnitDeploymentViewSetVisible(deployment, want_open);
}

///
/// Zeichnet einen gefuellten Ring zwischen "inner_radius" und "outer_radius"
/// aus RAD_CONTEXT_MENU_VIEW_SEGMENTS Vierecken, je zwei Dreiecke, in einem
/// Aufruf von SDL_RenderGeometry.
///
static void RAD_ContextMenuViewFillRing(SDL_Renderer *renderer, float center_x, float center_y, float inner_radius, float outer_radius, SDL_Color color)
{
    // Platz 2i ist der aeussere, 2i + 1 der innere Punkt des i-ten Strahls.
    SDL_Vertex vertices[2 * RAD_CONTEXT_MENU_VIEW_SEGMENTS];
    int indices[6 * RAD_CONTEXT_MENU_VIEW_SEGMENTS];

    for(int i = 0; i < RAD_CONTEXT_MENU_VIEW_SEGMENTS; i++)
    {
        const float angle = 2.0f * (float)M_PI * (float)i / (float)RAD_CONTEXT_MENU_VIEW_SEGMENTS;
        const float cos_angle = cosf(angle);
        const float sin_angle = sinf(angle);
        vertices[2 * i] = (SDL_Vertex){
            .position = { .x = center_x + outer_radius * cos_angle, .y = center_y + outer_radius * sin_angle },
            .color = color,
            .tex_coord = { .x = 0.0f, .y = 0.0f }
        };
        vertices[2 * i + 1] = (SDL_Vertex){
            .position = { .x = center_x + inner_radius * cos_angle, .y = center_y + inner_radius * sin_angle },
            .color = color,
            .tex_coord = { .x = 0.0f, .y = 0.0f }
        };

        // Das Viereck zwischen diesem Strahl und dem naechsten.
        const int outer = 2 * i;
        const int inner = 2 * i + 1;
        const int next_outer = 2 * ((i + 1) % RAD_CONTEXT_MENU_VIEW_SEGMENTS);
        const int next_inner = next_outer + 1;
        indices[6 * i] = outer;
        indices[6 * i + 1] = next_outer;
        indices[6 * i + 2] = inner;
        indices[6 * i + 3] = inner;
        indices[6 * i + 4] = next_outer;
        indices[6 * i + 5] = next_inner;
    }

    SDL_RenderGeometry(renderer, NULL, vertices, 2 * RAD_CONTEXT_MENU_VIEW_SEGMENTS, indices, 6 * RAD_CONTEXT_MENU_VIEW_SEGMENTS);
}

///
/// Zeichnet eine gefuellte Raute in Groesse eines Feldes
/// (RAD_ISO_TILE_WIDTH x RAD_ISO_TILE_HEIGHT) um (center_x, center_y), aus
/// zwei Dreiecken in einem Aufruf von SDL_RenderGeometry. Den Blend-Modus
/// setzt der Aufrufer.
///
static void RAD_ContextMenuViewFillTile(SDL_Renderer *renderer, float center_x, float center_y, SDL_Color color)
{
    const float half_width = 0.5f * (float)RAD_ISO_TILE_WIDTH;
    const float half_height = 0.5f * (float)RAD_ISO_TILE_HEIGHT;

    // Links, oben, rechts, unten.
    const SDL_Vertex vertices[4] = {
        { .position = { .x = center_x - half_width, .y = center_y }, .color = color, .tex_coord = { .x = 0.0f, .y = 0.0f } },
        { .position = { .x = center_x, .y = center_y - half_height }, .color = color, .tex_coord = { .x = 0.0f, .y = 0.0f } },
        { .position = { .x = center_x + half_width, .y = center_y }, .color = color, .tex_coord = { .x = 0.0f, .y = 0.0f } },
        { .position = { .x = center_x, .y = center_y + half_height }, .color = color, .tex_coord = { .x = 0.0f, .y = 0.0f } }
    };
    const int indices[6] = { 0, 1, 2, 0, 2, 3 };

    SDL_RenderGeometry(renderer, NULL, vertices, 4, indices, 6);
}

///
/// Deckt wenigstens eine Waffe der Einheit die Entfernung "distance" ab, in
/// Feldern waagerecht plus senkrecht, zwischen min_range und max_range
/// (attack_ranges)? Dieselbe Frage fuer die Zielwahl
/// (RAD_ContextMenuViewSelectTarget) und die Markierung
/// (RAD_ContextMenuViewDrawAttackRange) -- so markiert sie genau, was sie als
/// Ziel annimmt.
///
static bool RAD_ContextMenuViewInRange(const RAD_ContextMenuView_t *context_menu_view, int32_t distance)
{
    for(int32_t i = 0; i < context_menu_view->attack_range_count; i++)
    {
        const RAD_ContextMenuViewWeaponRange_t range = context_menu_view->attack_ranges[i];
        if(distance >= range.min_range && distance <= range.max_range)
        {
            return true;
        }
    }
    return false;
}

///
/// Markiert den Weg der ziehenden Einheit: jedes Feld gelb, das Ziel
/// kraeftiger, das Feld der Einheit nicht. Den Blend-Modus setzt der Aufrufer.
///
static void RAD_ContextMenuViewDrawPath(const RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer)
{
    const RAD_NetPath_t *path = &context_menu_view->move_path;

    // Ab 1: das Feld der Einheit bleibt unmarkiert.
    for(int8_t i = 1; i < path->number_of_steps; i++)
    {
        const bool target = (i == path->number_of_steps - 1);
        RAD_ContextMenuViewMarkTile(context_menu_view, renderer, path->steps_to[i].x, path->steps_to[i].y,
                                    target ? RAD_CONTEXT_MENU_VIEW_PATH_TARGET_COLOR : RAD_CONTEXT_MENU_VIEW_PATH_COLOR);
    }
}

///
/// Markiert die Felder in Reichweite der angreifenden Einheit rot
/// (RAD_ContextMenuViewInRange), das Feld der Einheit nicht -- und mit
/// "with_target" das gewaehlte Ziel kraeftiger. Abgesucht wird die Raute bis zur
/// groessten Reichweite um die Einheit; was die Karte nicht zeichnet, faellt weg
/// (RAD_ContextMenuViewMarkTile). Ob dort eine eigene Einheit steht, weiss die
/// ContextMenuView nicht -- ein solches Feld ist markiert, als Ziel aber
/// abgelehnt (RAD_ContextMenuViewSelectTarget). Den Blend-Modus setzt der
/// Aufrufer.
///
static void RAD_ContextMenuViewDrawAttackRange(const RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer, bool with_target)
{
    int32_t reach = 0;
    for(int32_t i = 0; i < context_menu_view->attack_range_count; i++)
    {
        if(context_menu_view->attack_ranges[i].max_range > reach)
        {
            reach = context_menu_view->attack_ranges[i].max_range;
        }
    }

    for(int32_t dy = -reach; dy <= reach; dy++)
    {
        for(int32_t dx = -reach; dx <= reach; dx++)
        {
            const int32_t distance = abs(dx) + abs(dy);
            if(distance == 0 || !RAD_ContextMenuViewInRange(context_menu_view, distance))
            {
                continue;
            }

            const int32_t x = context_menu_view->tile_x + dx;
            const int32_t y = context_menu_view->tile_y + dy;
            const bool target = with_target && (x == context_menu_view->target_x) && (y == context_menu_view->target_y);
            RAD_ContextMenuViewMarkTile(context_menu_view, renderer, x, y,
                                        target ? RAD_CONTEXT_MENU_VIEW_RANGE_TARGET_COLOR : RAD_CONTEXT_MENU_VIEW_RANGE_COLOR);
        }
    }
}

///
/// Liegt (x, y) in der Aufstellzone des Spielers dieses Clients, wie der Server
/// sie geschickt hat (RAD_ClientWorldInDeployZone)? false, solange keine da ist.
/// Dieselbe Frage fuer die Markierung (RAD_ContextMenuViewDrawDeployZone) und die
/// Bestaetigung (RAD_ContextMenuViewRunAction) -- so schickt sie genau, was sie
/// als annehmbar markiert.
///
static bool RAD_ContextMenuViewInDeployZone(const RAD_ContextMenuView_t *context_menu_view, int32_t x, int32_t y)
{
    if(context_menu_view->world == NULL || context_menu_view->session == NULL)
    {
        return false;
    }
    return RAD_ClientWorldInDeployZone(context_menu_view->world,
                                       (RAD_ClientPlayerId_t)context_menu_view->session->own_player_id, x, y);
}

///
/// Warum der Server ein Deploy auf (x, y) ablehnen wuerde, NULL, wenn er es
/// annimmt -- dieselben Gruende in derselben Reihenfolge wie im Spiel
/// (RAD_GameCheckDeployUnit), soweit sie am Feld haengen:
///
///   - ausserhalb der eigenen Aufstellzone (RAD_ContextMenuViewInDeployZone)
///   - ein Feld, das der Client nicht kennt
///   - kein Gelaende oder unbegehbar (RAD_ControlTerrainImpassable)
///   - schon besetzt
///
/// Ein statischer Text fuers Log. Dieselbe Frage fuer die Markierung
/// (RAD_ContextMenuViewDrawDeployZone), die Hervorhebung von Deploy und die
/// Bestaetigung (RAD_ContextMenuViewRunAction) -- so schickt sie genau, was
/// sie als annehmbar markiert.
///
static const char* RAD_ContextMenuViewDeployRejection(const RAD_ContextMenuView_t *context_menu_view, int32_t x, int32_t y)
{
    if(!RAD_ContextMenuViewInDeployZone(context_menu_view, x, y))
    {
        return "ausserhalb der Aufstellzone";
    }

    const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt(context_menu_view->world, x, y);
    if(tile == NULL)
    {
        return "Feld unbekannt";
    }
    if(tile->type == RAD_CLIENT_TILE_TYPE_VOID)
    {
        return "kein Gelaende";
    }
    if(RAD_ControlTerrainImpassable(tile))
    {
        return "unbegehbar";
    }
    if(RAD_ClientTileHasUnit(tile))
    {
        return "besetzt";
    }
    return NULL;
}

///
/// Markiert die Felder der eigenen Aufstellzone gruen, auf die sich aufstellen
/// laesst -- unbegehbare, besetzte und solche ohne Gelaende bleiben frei -- und
/// das ausgewaehlte Feld kraeftiger: gruen, wenn Deploy es annimmt, rot, wenn
/// nicht (RAD_ContextMenuViewDeployRejection). Ohne Zone bleibt es beim
/// ausgewaehlten Feld, rot. Was die Karte
/// nicht zeichnet, faellt weg (RAD_ContextMenuViewMarkTile). Den Blend-Modus
/// setzt der Aufrufer.
///
static void RAD_ContextMenuViewDrawDeployZone(const RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer)
{
    const RAD_ClientDeployZone_t *zone = NULL;
    if(context_menu_view->world != NULL && context_menu_view->session != NULL)
    {
        zone = RAD_ClientWorldDeployZone(context_menu_view->world,
                                         (RAD_ClientPlayerId_t)context_menu_view->session->own_player_id);
    }

    const int32_t tile_x = context_menu_view->tile_x;
    const int32_t tile_y = context_menu_view->tile_y;
    if(zone != NULL)
    {
        for(int32_t y = zone->min_y; y <= zone->max_y; y++)
        {
            for(int32_t x = zone->min_x; x <= zone->max_x; x++)
            {
                // Das ausgewaehlte Feld bekommt unten seine eigene Farbe.
                if((x == tile_x && y == tile_y) || RAD_ContextMenuViewDeployRejection(context_menu_view, x, y) != NULL)
                {
                    continue;
                }
                RAD_ContextMenuViewMarkTile(context_menu_view, renderer, x, y, RAD_CONTEXT_MENU_VIEW_DEPLOY_ZONE_COLOR);
            }
        }
    }

    const bool accepted = (RAD_ContextMenuViewDeployRejection(context_menu_view, tile_x, tile_y) == NULL);
    RAD_ContextMenuViewMarkTile(context_menu_view, renderer, tile_x, tile_y,
                                accepted ? RAD_CONTEXT_MENU_VIEW_DEPLOY_TARGET_COLOR : RAD_CONTEXT_MENU_VIEW_DEPLOY_OUTSIDE_TARGET_COLOR);
}

///
/// Markiert das Feld (x, y) in "color" (RAD_ContextMenuViewFillTile), wenn die
/// Karte es zeichnet (RAD_IsoMapTileScreenCenter); sonst nichts.
///
static void RAD_ContextMenuViewMarkTile(const RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer, int32_t x, int32_t y, SDL_Color color)
{
    int32_t center_x = 0, center_y = 0;
    if(RAD_IsoMapTileScreenCenter(context_menu_view->map, x, y, &center_x, &center_y))
    {
        RAD_ContextMenuViewFillTile(renderer, (float)center_x, (float)center_y, color);
    }
}

///
/// Markiert blau, welche Felder die ziehende Einheit vom Ende ihres Weges aus
/// noch erreicht -- nach denselben Regeln, nach denen ein Klick den Weg
/// verlaengert (RAD_ContextMenuViewExtendPath): Schritt fuer Schritt zu einem
/// waagerechten oder senkrechten Nachbarn, nur auf bekannte, freie und begehbare
/// Felder, die noch nicht im Weg sind, mit den Kosten des Terrains
/// (RAD_ControlTerrainMovementCost) im verbleibenden Budget und hoechstens bis
/// RAD_NET_PATH_MAX_STEPS Felder. Das Ende des Weges selbst nicht.
///
/// Gesucht wird in Schichten: nach k Durchgaengen steht in "cost", was jedes
/// Feld mit hoechstens k Schritten mindestens kostet. So zaehlen Kosten und
/// Schritte beide, und ein teurerer Umweg mit weniger Schritten geht nicht
/// verloren. Den Blend-Modus setzt der Aufrufer.
///
static void RAD_ContextMenuViewDrawReach(const RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer)
{
    const RAD_NetPath_t *path = &context_menu_view->move_path;
    if(path->number_of_steps <= 0 || context_menu_view->world == NULL)
    {
        return;
    }

    const int32_t budget = context_menu_view->move_budget - context_menu_view->move_cost;
    const int32_t steps = RAD_NET_PATH_MAX_STEPS - path->number_of_steps;
    const int32_t origin_x = path->steps_to[path->number_of_steps - 1].x;
    const int32_t origin_y = path->steps_to[path->number_of_steps - 1].y;
    const int32_t radius = RAD_CONTEXT_MENU_VIEW_REACH_RADIUS;
    if(budget <= 0 || steps <= 0)
    {
        return;
    }

    // Unerreicht ist teurer als jedes Budget.
    int32_t cost[RAD_CONTEXT_MENU_VIEW_REACH_SIDE][RAD_CONTEXT_MENU_VIEW_REACH_SIDE];
    for(int32_t j = 0; j < RAD_CONTEXT_MENU_VIEW_REACH_SIDE; j++)
    {
        for(int32_t i = 0; i < RAD_CONTEXT_MENU_VIEW_REACH_SIDE; i++)
        {
            cost[j][i] = INT32_MAX;
        }
    }
    cost[radius][radius] = 0;

    static const int32_t neighbours[4][2] = { { 1, 0 }, { -1, 0 }, { 0, 1 }, { 0, -1 } };
    for(int32_t k = 0; k < steps; k++)
    {
        // Eine Schicht weiter: gelesen wird der Stand davor, damit ein Feld in
        // diesem Durchgang nur einen Schritt weiter kommt.
        int32_t next[RAD_CONTEXT_MENU_VIEW_REACH_SIDE][RAD_CONTEXT_MENU_VIEW_REACH_SIDE];
        memcpy(next, cost, sizeof(next));

        for(int32_t j = 0; j < RAD_CONTEXT_MENU_VIEW_REACH_SIDE; j++)
        {
            for(int32_t i = 0; i < RAD_CONTEXT_MENU_VIEW_REACH_SIDE; i++)
            {
                if(cost[j][i] == INT32_MAX)
                {
                    continue;
                }

                for(int32_t n = 0; n < 4; n++)
                {
                    const int32_t ni = i + neighbours[n][0];
                    const int32_t nj = j + neighbours[n][1];
                    if(ni < 0 || nj < 0 || ni >= RAD_CONTEXT_MENU_VIEW_REACH_SIDE || nj >= RAD_CONTEXT_MENU_VIEW_REACH_SIDE)
                    {
                        continue;
                    }

                    const int32_t x = origin_x + ni - radius;
                    const int32_t y = origin_y + nj - radius;
                    const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt(context_menu_view->world, x, y);
                    if(tile == NULL || RAD_ClientTileHasUnit(tile) || RAD_ControlTerrainImpassable(tile)
                       || RAD_ContextMenuViewPathContains(context_menu_view, x, y))
                    {
                        continue;
                    }

                    const int32_t reached = cost[j][i] + RAD_ControlTerrainMovementCost(tile);
                    if(reached <= budget && reached < next[nj][ni])
                    {
                        next[nj][ni] = reached;
                    }
                }
            }
        }

        memcpy(cost, next, sizeof(cost));
    }

    for(int32_t j = 0; j < RAD_CONTEXT_MENU_VIEW_REACH_SIDE; j++)
    {
        for(int32_t i = 0; i < RAD_CONTEXT_MENU_VIEW_REACH_SIDE; i++)
        {
            if(cost[j][i] == INT32_MAX || (i == radius && j == radius))
            {
                continue;
            }
            RAD_ContextMenuViewMarkTile(context_menu_view, renderer, origin_x + i - radius, origin_y + j - radius,
                                        RAD_CONTEXT_MENU_VIEW_REACH_COLOR);
        }
    }
}

///
/// Steht (x, y) schon im Weg der ziehenden Einheit?
///
static bool RAD_ContextMenuViewPathContains(const RAD_ContextMenuView_t *context_menu_view, int32_t x, int32_t y)
{
    const RAD_NetPath_t *path = &context_menu_view->move_path;
    for(int8_t i = 0; i < path->number_of_steps; i++)
    {
        if(path->steps_to[i].x == x && path->steps_to[i].y == y)
        {
            return true;
        }
    }
    return false;
}
