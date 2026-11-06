#include <radish/view/iso_map_view.h>
#include <radish/rendering/iso_effect.h>
#include <radish/rendering/iso_unit_images.h>
#include <radish/view/animations/unit_animation.h>
#include <radish/control/visibility.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


///
/// Wie viele Zuege sie gleichzeitig selbst darstellt, wie viele Felder ein Weg
/// dafuer hoechstens haben darf. Was darueber hinausgeht, springt wie zuvor. Wie
/// lange ein Schritt von Feld zu Feld und eine Vierteldrehung dauern, sagt das
/// Bild der Figur (RAD_IsoUnitStepMs, RAD_IsoUnitTurnMs).
///
#define RAD_ISO_MAP_VIEW_MOVES_MAX 4
#define RAD_ISO_MAP_VIEW_MOVE_STEPS_MAX 16

///
/// Wie viele Figuren gleichzeitig zielen koennen, ohne zu ziehen. Was darueber
/// hinausgeht, zielt nicht.
///
#define RAD_ISO_MAP_VIEW_AIMS_MAX 4

///
/// Wie viele Explosionen gleichzeitig laufen koennen. Was darueber hinausgeht,
/// explodiert nicht.
///
#define RAD_ISO_MAP_VIEW_EXPLOSIONS_MAX 8

///
/// Wie lange ein Feld braucht, um ganz in den Nebel zu fallen oder ganz aus ihm
/// zu kommen (RAD_IsoMapViewApplyFog).
///
#define RAD_ISO_MAP_VIEW_FOG_FADE_MS 180.0f

///
/// Ein Zug, den die IsoMapView selbst darstellt (RAD_IsoMapViewOnUnitMoved).
///
/// **Die Entitaet ist von ihrem Feld geloest**, solange er laeuft: sie liegt
/// weder auf dem Start- noch auf dem Zielfeld und gehoert der IsoMapView. In der
/// Iso-Map steht sie als Figur ausserhalb eines Feldes (RAD_IsoMapAddEntity),
/// die Animation sagt ihre Position und Blickrichtung. Am Ende kommt sie aufs Zielfeld zurueck
/// (RAD_IsoMapViewFinishMove).
///
typedef struct
{
    bool active;
    RAD_ClientUnitId_t unit;

    ///
    /// Vom Start-Iso-Objekt geloest und in die Iso-Map eingetragen.
    ///
    RAD_IsoEntity_t *entity;

    ///
    /// Das Zielfeld: dort haelt sie die Iso-Map frei
    /// (RAD_IsoMapViewDetachMovingEntities), dort kommt "entity" am Ende hin.
    ///
    int32_t to_x;
    int32_t to_y;

    ///
    /// Fahrten und Drehungen; RAD_UpdateIsoMapView uebertraegt sie je Frame auf
    /// "entity".
    ///
    RAD_UnitAnimation_t *animation;
} RAD_IsoMapViewMove_t;

///
/// Eine Figur, die ihren Turm richtet, ohne zu ziehen (RAD_IsoMapViewAim): sie
/// bleibt auf ihrem Feld (x, y), die Animation sagt nur, wie weit ihr Turm
/// gedreht ist. Die Entitaet gehoert der Iso-Map und wird je Frame dort
/// nachgeschlagen -- verschwindet sie, endet das Zielen.
///
typedef struct
{
    bool active;
    RAD_ClientUnitId_t unit;
    int32_t x;
    int32_t y;
    RAD_UnitAnimation_t *animation;
} RAD_IsoMapViewAim_t;

///
/// Eine Explosion auf dem Zielfeld eines Angriffs (RAD_IsoMapViewFire): sie
/// beginnt mit dem Schuss, bei "start_ms", und laeuft einmal durch
/// (RAD_IsoEffectDurationMs). Bis dahin wartet sie; in der Iso-Map steht sie
/// nur, solange sie laeuft ("shown", RAD_IsoMapAddEffect).
///
typedef struct
{
    bool active;
    bool shown;
    float start_ms;
    RAD_IsoEffect_t effect;
} RAD_IsoMapViewExplosion_t;

///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_IsoMapView
{
    SDL_Rect area;
    RAD_IsoMap_t *map;
    const RAD_ClientWorld_t *world;

    ///
    /// Die Zuege, die gerade laufen; frei, wo "active" nicht gesetzt ist.
    ///
    RAD_IsoMapViewMove_t moves[RAD_ISO_MAP_VIEW_MOVES_MAX];

    ///
    /// Die Figuren, die gerade zielen, ohne zu ziehen; frei, wo "active" nicht
    /// gesetzt ist. Eine Einheit steht nie zugleich hier und in "moves".
    ///
    RAD_IsoMapViewAim_t aims[RAD_ISO_MAP_VIEW_AIMS_MAX];

    ///
    /// Die Explosionen, die warten oder laufen; frei, wo "active" nicht gesetzt
    /// ist. Ihre Effekte stehen in der Iso-Map, gehoeren aber hierher.
    ///
    RAD_IsoMapViewExplosion_t explosions[RAD_ISO_MAP_VIEW_EXPLOSIONS_MAX];

    ///
    /// Das Iso-Objekt unter dem Mauszeiger (RAD_IsoMapViewHandleMouseMotion), mit
    /// gesetztem "focus" (rendering/iso_object.h); NULL, wenn keines. Gehoert der
    /// Iso-Map.
    ///
    RAD_IsoObject_t *focus_object;

    ///
    /// Das Feld der Welt zu "focus_object" (model/focus.h). Gehoert dem Aufrufer.
    ///
    RAD_ClientFocus_t *focus;

    ///
    /// Die Verbindung, aus der die eigene Spieler-Id kommt (own_player_id) --
    /// von ihr aus liegt der Nebel (RAD_IsoMapViewApplyFog). Gehoert dem Aufrufer.
    ///
    const RAD_IoNetSession_t *session;

    ///
    /// Die Helligkeit jedes Feldes, wie sie gerade gezeichnet wird -- sie laeuft
    /// je Bild auf das zu, was die Sicht verlangt (RAD_IsoMapViewApplyFog) --,
    /// und wann zuletzt geblendet wurde; negativ, solange noch nie.
    ///
    float light[RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];
    float fog_ms;
};


static bool RAD_IsoMapViewObserveWorld(RAD_IsoMapView_t *iso_map_view);
static void RAD_IsoMapViewStopObservingWorld(RAD_IsoMapView_t *iso_map_view);
static void RAD_IsoMapViewClearFocus(RAD_IsoMapView_t *iso_map_view);
static void RAD_IsoMapViewDetachMovingEntities(RAD_IsoMapView_t *iso_map_view, const RAD_ClientTile_t *tile);
static void RAD_IsoMapViewApplyTile(RAD_IsoMapView_t *iso_map_view, const RAD_ClientTile_t *tile);
static int32_t RAD_IsoMapViewPlayerOf(const RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit);
static void RAD_IsoMapViewFaceEnemy(RAD_IsoMapView_t *iso_map_view, const RAD_ClientTile_t *tile);
static void RAD_IsoMapViewFinishMove(RAD_IsoMapView_t *iso_map_view, RAD_IsoMapViewMove_t *move);
static void RAD_IsoMapViewStopAim(RAD_IsoMapViewAim_t *aim);
static void RAD_IsoMapViewApplyFog(RAD_IsoMapView_t *iso_map_view, float now_ms);
static void RAD_IsoMapViewUpdateExplosions(RAD_IsoMapView_t *iso_map_view, float now_ms);
static void RAD_IsoMapViewStopExplosion(RAD_IsoMapView_t *iso_map_view, RAD_IsoMapViewExplosion_t *explosion);
static RAD_IsoEntity_t* RAD_IsoMapViewEntityAt(const RAD_IsoMapView_t *iso_map_view, int32_t x, int32_t y,
                                               RAD_ClientUnitId_t unit);
static float RAD_IsoMapViewNow(void);
static void RAD_IsoMapViewLogAttackResult(const RAD_ClientAttackResult_t *result);


RAD_IsoMapView_t* RAD_CreateIsoMapView(int32_t x, int32_t y, int32_t width, int32_t height, RAD_IsoMap_t *map,
                                       const RAD_ClientWorld_t *world, RAD_ClientFocus_t *focus,
                                       const RAD_IoNetSession_t *session)
{
    RAD_IsoMapView_t *iso_map_view = malloc(sizeof(struct RAD_IsoMapView));
    if(iso_map_view == NULL)
    {
        return NULL;
    }

    *iso_map_view = (struct RAD_IsoMapView){
        .area = { .x = x, .y = y, .w = width, .h = height },
        .map = map,
        .world = world,
        .focus_object = NULL,
        .focus = focus,
        .session = session,
        .fog_ms = -1.0f
    };
    for(int32_t y = 0; y < RAD_ISO_MAP_SIZE; ++y)
    {
        for(int32_t x = 0; x < RAD_ISO_MAP_SIZE; ++x)
        {
            iso_map_view->light[y][x] = 255.0f;
        }
    }

    if(!RAD_IsoMapViewObserveWorld(iso_map_view))
    {
        free(iso_map_view);
        return NULL;
    }

    return iso_map_view;
}

void RAD_DestroyIsoMapView(RAD_IsoMapView_t **iso_map_view)
{
    RAD_IsoMapViewStopObservingWorld(*iso_map_view);

    // Die Iso-Map ueberlebt die IsoMapView: laufende Zuege legen ihre Entitaet
    // zurueck, so steht die Karte wieder, wie die Welt es sagt.
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_MOVES_MAX; ++i)
    {
        if((*iso_map_view)->moves[i].active)
        {
            RAD_IsoMapViewFinishMove(*iso_map_view, &(*iso_map_view)->moves[i]);
        }
    }
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_AIMS_MAX; ++i)
    {
        if((*iso_map_view)->aims[i].active)
        {
            RAD_IsoMapViewStopAim(&(*iso_map_view)->aims[i]);
        }
    }
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_EXPLOSIONS_MAX; ++i)
    {
        if((*iso_map_view)->explosions[i].active)
        {
            RAD_IsoMapViewStopExplosion(*iso_map_view, &(*iso_map_view)->explosions[i]);
        }
    }

    RAD_IsoMapViewClearFocus(*iso_map_view);
    free(*iso_map_view);
    *iso_map_view = NULL;
}

void RAD_UpdateIsoMapView(RAD_IsoMapView_t *iso_map_view, SDL_Renderer *renderer)
{
    // Zuerst die Animationen: sie schreiben die Position der Figuren in Bewegung,
    // die RAD_RenderIsoMap gleich zeichnet. Ein fertiger Zug legt seine Figur
    // zurueck aufs Zielfeld, noch bevor gezeichnet wird.
    const float now_ms = RAD_IsoMapViewNow();
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_MOVES_MAX; ++i)
    {
        RAD_IsoMapViewMove_t *move = &iso_map_view->moves[i];
        if(!move->active)
        {
            continue;
        }

        const RAD_UnitAnimationState_t state = RAD_UnitAnimationStateAt(move->animation, now_ms);
        move->entity->x = state.x;
        move->entity->y = state.y;
        move->entity->facing = state.facing;
        move->entity->facing_set = true;
        move->entity->turning = state.turning;
        move->entity->turn_to = state.turn_to;
        move->entity->turn_progress = state.turn_progress;
        move->entity->turret_angle = state.turret_angle;

        if(RAD_UnitAnimationIsFinished(move->animation, now_ms))
        {
            RAD_IsoMapViewFinishMove(iso_map_view, move);
        }
    }

    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_AIMS_MAX; ++i)
    {
        RAD_IsoMapViewAim_t *aim = &iso_map_view->aims[i];
        if(!aim->active)
        {
            continue;
        }

        RAD_IsoEntity_t *entity = RAD_IsoMapViewEntityAt(iso_map_view, aim->x, aim->y, aim->unit);
        if(entity != NULL)
        {
            entity->turret_angle = RAD_UnitAnimationStateAt(aim->animation, now_ms).turret_angle;
        }
        if((entity == NULL) || RAD_UnitAnimationIsFinished(aim->animation, now_ms))
        {
            RAD_IsoMapViewStopAim(aim);
        }
    }

    RAD_IsoMapViewUpdateExplosions(iso_map_view, now_ms);

    // Der Nebel je Bild neu: die Sicht haengt an jedem Zug, jedem Aufstellen und
    // jeder Einheit, die kommt oder geht -- und sie zu rechnen kostet weniger,
    // als all das zu beobachten (control/visibility.h). Nach den Zuegen oben,
    // damit sie von dort aus sieht, wo die Figur in diesem Bild steht.
    RAD_IsoMapViewApplyFog(iso_map_view, now_ms);

    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
    SDL_RenderFillRect(renderer, &iso_map_view->area);

    // Abschneiden, was ueber die Flaeche hinausgeht; danach die Clip-Flaeche des
    // Aufrufers wiederherstellen.
    const SDL_bool was_clipping = SDL_RenderIsClipEnabled(renderer);
    SDL_Rect previous_clip;
    SDL_RenderGetClipRect(renderer, &previous_clip);
    SDL_RenderSetClipRect(renderer, &iso_map_view->area);

    RAD_RenderIsoMap(renderer, iso_map_view->map);

    SDL_RenderSetClipRect(renderer, was_clipping ? &previous_clip : NULL);
}

bool RAD_IsoMapViewIsBusy(const RAD_IsoMapView_t *iso_map_view)
{
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_MOVES_MAX; ++i)
    {
        if(iso_map_view->moves[i].active)
        {
            return true;
        }
    }

    // Nur ein Zielen, das feuert: das ohne Schuss ist die Vorschau aus der
    // ContextMenuView und kommt nicht vom Server.
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_AIMS_MAX; ++i)
    {
        const RAD_IsoMapViewAim_t *aim = &iso_map_view->aims[i];
        float fire_ms;
        if(aim->active && (aim->animation != NULL) && RAD_UnitAnimationFireMs(aim->animation, &fire_ms))
        {
            return true;
        }
    }

    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_EXPLOSIONS_MAX; ++i)
    {
        if(iso_map_view->explosions[i].active)
        {
            return true;
        }
    }

    return false;
}

void RAD_IsoMapViewHandleMouseMotion(RAD_IsoMapView_t *iso_map_view, const SDL_MouseMotionEvent *motion)
{
    RAD_IsoObject_t *focus_object = NULL;

    const SDL_Point pointer = { .x = motion->x, .y = motion->y };
    if(SDL_PointInRect(&pointer, &iso_map_view->area))
    {
        focus_object = RAD_IsoObjectAtScreenCoordinates(iso_map_view->map, motion->x, motion->y);
        // Ein Feld, das der Server noch nicht geschickt hat, wird nicht gezeichnet.
        if(focus_object != NULL && !focus_object->present)
        {
            focus_object = NULL;
        }
    }

    if(focus_object != iso_map_view->focus_object)
    {
        RAD_IsoMapViewClearFocus(iso_map_view);
        iso_map_view->focus_object = focus_object;

        if(focus_object == NULL)
        {
            printf("[IsoMapView] Maus ueber keinem Feld\n");
        }
        else
        {
            iso_map_view->focus->tile = &iso_map_view->world->tiles[focus_object->y][focus_object->x];
            printf("[IsoMapView] Maus ueber Feld (%d, %d)\n", (int)focus_object->x, (int)focus_object->y);
        }
    }

    // Auch ohne Wechsel: RAD_IsoMapApplyTile legt ein Objekt womoeglich neu an und
    // setzt "focus" damit zurueck.
    if(focus_object != NULL)
    {
        focus_object->focus = true;
    }
}

///
/// Nimmt den Fokus weg: das Iso-Objekt wird nicht mehr hervorgehoben, im Fokus
/// steht nichts mehr.
///
static void RAD_IsoMapViewClearFocus(RAD_IsoMapView_t *iso_map_view)
{
    if(iso_map_view->focus_object != NULL)
    {
        iso_map_view->focus_object->focus = false;
        iso_map_view->focus_object = NULL;
    }
    iso_map_view->focus->tile = NULL;
}


//
// Beobachter der Welt (view/iso_map_view.h). "user_argument" ist bei allen die
// IsoMapView selbst. Felder gehen in die Iso-Map, alles andere vorerst nur ins
// Log.
//

static void RAD_IsoMapViewOnTileChanged(void *user_argument, const RAD_ClientTile_t *tile)
{
    RAD_IsoMapView_t *iso_map_view = (RAD_IsoMapView_t*)user_argument;
    printf("[IsoMapView] Feld (%d, %d) geaendert, Typ=%s\n", (int)tile->x, (int)tile->y, RAD_ClientTileTypeText(tile->type));

    RAD_IsoMapViewApplyTile(iso_map_view, tile);
    RAD_IsoMapViewDetachMovingEntities(iso_map_view, tile);
    RAD_IsoMapViewFaceEnemy(iso_map_view, tile);
}

static void RAD_IsoMapViewOnTileRemoved(void *user_argument, const RAD_ClientTile_t *tile)
{
    RAD_IsoMapView_t *iso_map_view = (RAD_IsoMapView_t*)user_argument;
    printf("[IsoMapView] Feld (%d, %d) verworfen\n", (int)tile->x, (int)tile->y);

    if(iso_map_view->focus->tile == tile)
    {
        RAD_IsoMapViewClearFocus(iso_map_view);
    }

    RAD_IsoMapRemoveTile(iso_map_view->map, (uint32_t)tile->x, (uint32_t)tile->y);
}

static void RAD_IsoMapViewOnTileCreated(void *user_argument, const RAD_ClientWorld_t *world, const RAD_ClientTile_t *tile)
{
    (void)world;
    RAD_IsoMapView_t *iso_map_view = (RAD_IsoMapView_t*)user_argument;
    printf("[IsoMapView] Feld (%d, %d) neu, Typ=%s\n", (int)tile->x, (int)tile->y, RAD_ClientTileTypeText(tile->type));

    RAD_IsoMapViewApplyTile(iso_map_view, tile);
    RAD_IsoMapViewDetachMovingEntities(iso_map_view, tile);
    RAD_IsoMapViewFaceEnemy(iso_map_view, tile);

    // Aenderungen und Verwerfen meldet nur das Feld selbst. Nach "removed" ist die
    // IsoMapView dort abgemeldet; kommt das Feld wieder, meldet sie sich hier neu an.
    if(!RAD_ClientTileSubscribe(tile, (RAD_ClientTileObserver_t){
        .user_argument = user_argument,
        .changed = RAD_IsoMapViewOnTileChanged,
        .removed = RAD_IsoMapViewOnTileRemoved
    }))
    {
        printf("[IsoMapView] Feld (%d, %d) nicht beobachtbar\n", (int)tile->x, (int)tile->y);
    }
}

///
/// Der Name der Einheit "id" fuers Log, "?" fuer eine, die die Welt nicht kennt.
///
static const char* RAD_IsoMapViewUnitName(const RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t id)
{
    const RAD_ClientUnit_t *unit = RAD_ClientUnitRepositoryFindConst(RAD_ClientWorldUnits(iso_map_view->world), id);
    return (unit != NULL) ? unit->name : "?";
}

static void RAD_IsoMapViewOnReserveUnitAdded(void *user_argument, const RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id)
{
    printf("[IsoMapView] Reserve 0x%llx: Einheit %d \"%s\" neu (%u Einheiten)\n",
           (unsigned long long)reserve->owner, (int)id, RAD_IsoMapViewUnitName(user_argument, id),
           (unsigned)RAD_ClientReserveNumberOfUnits(reserve));
}

static void RAD_IsoMapViewOnReserveUnitRemoved(void *user_argument, const RAD_ClientReserve_t *reserve, RAD_ClientUnitId_t id)
{
    printf("[IsoMapView] Reserve 0x%llx: Einheit %d \"%s\" entfernt (%u Einheiten)\n",
           (unsigned long long)reserve->owner, (int)id, RAD_IsoMapViewUnitName(user_argument, id),
           (unsigned)RAD_ClientReserveNumberOfUnits(reserve));
}

static void RAD_IsoMapViewOnReserveCreated(void *user_argument, const RAD_ClientWorld_t *world, const RAD_ClientReserve_t *reserve)
{
    (void)world;
    printf("[IsoMapView] Reserve fuer 0x%llx neu\n", (unsigned long long)reserve->owner);

    if(!RAD_ClientReserveSubscribe(reserve, (RAD_ClientReserveObserver_t){
        .user_argument = user_argument,
        .unit_added = RAD_IsoMapViewOnReserveUnitAdded,
        .unit_removed = RAD_IsoMapViewOnReserveUnitRemoved
    }))
    {
        printf("[IsoMapView] Reserve fuer 0x%llx nicht beobachtbar\n", (unsigned long long)reserve->owner);
    }
}

///
/// Die Mitspieler sind da oder anders: jede Figur bekommt den Ring ihres
/// Besitzers neu, auf dem Feld (RAD_IsoMapViewApplyTile) wie im Zug.
///
static void RAD_IsoMapViewOnPlayersChanged(void *user_argument, const RAD_ClientWorld_t *world)
{
    RAD_IsoMapView_t *iso_map_view = (RAD_IsoMapView_t*)user_argument;
    printf("[IsoMapView] Mitspieler geaendert (%d)\n", (int)world->number_of_players);

    for(int32_t y = 0; y < RAD_ISO_MAP_SIZE; ++y)
    {
        for(int32_t x = 0; x < RAD_ISO_MAP_SIZE; ++x)
        {
            const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt(world, x, y);
            if((tile != NULL) && (tile->unit != NULL))
            {
                RAD_IsoMapViewApplyTile(iso_map_view, tile);
                RAD_IsoMapViewDetachMovingEntities(iso_map_view, tile);
            }
        }
    }

    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_MOVES_MAX; ++i)
    {
        RAD_IsoMapViewMove_t *move = &iso_map_view->moves[i];
        if(move->active)
        {
            move->entity->player = RAD_IsoMapViewPlayerOf(iso_map_view, move->unit);
        }
    }
}

static void RAD_IsoMapViewOnWorldSizeChanged(void *user_argument, const RAD_ClientWorld_t *world)
{
    (void)user_argument;
    printf("[IsoMapView] Weltgroesse %u x %u\n", (unsigned)world->width, (unsigned)world->height);
}

///
/// Der laufende Zug der Einheit "unit", oder NULL.
///
static RAD_IsoMapViewMove_t* RAD_IsoMapViewMoveOf(RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit)
{
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_MOVES_MAX; ++i)
    {
        if(iso_map_view->moves[i].active && (iso_map_view->moves[i].unit == unit))
        {
            return &iso_map_view->moves[i];
        }
    }
    return NULL;
}

static RAD_IsoMapViewMove_t* RAD_IsoMapViewFreeMove(RAD_IsoMapView_t *iso_map_view)
{
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_MOVES_MAX; ++i)
    {
        if(!iso_map_view->moves[i].active)
        {
            return &iso_map_view->moves[i];
        }
    }
    return NULL;
}

///
/// Das laufende Zielen der Einheit "unit", ohne Zug, oder NULL.
///
static RAD_IsoMapViewAim_t* RAD_IsoMapViewAimOf(RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit)
{
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_AIMS_MAX; ++i)
    {
        if(iso_map_view->aims[i].active && (iso_map_view->aims[i].unit == unit))
        {
            return &iso_map_view->aims[i];
        }
    }
    return NULL;
}

static RAD_IsoMapViewAim_t* RAD_IsoMapViewFreeAim(RAD_IsoMapView_t *iso_map_view)
{
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_AIMS_MAX; ++i)
    {
        if(!iso_map_view->aims[i].active)
        {
            return &iso_map_view->aims[i];
        }
    }
    return NULL;
}

///
/// Beendet ein Zielen ohne Zug: der Turm bleibt, wo er gerade steht.
///
static void RAD_IsoMapViewStopAim(RAD_IsoMapViewAim_t *aim)
{
    if(aim->animation != NULL)
    {
        RAD_DestroyUnitAnimation(&aim->animation);
    }
    *aim = (RAD_IsoMapViewAim_t){ .active = false };
}

///
/// Die Figur der Einheit "unit" auf Feld (x, y), oder NULL, wenn dort keine oder
/// eine andere liegt.
///
static RAD_IsoEntity_t* RAD_IsoMapViewEntityAt(const RAD_IsoMapView_t *iso_map_view, int32_t x, int32_t y,
                                               RAD_ClientUnitId_t unit)
{
    if(x < 0 || y < 0 || x >= RAD_ISO_MAP_SIZE || y >= RAD_ISO_MAP_SIZE)
    {
        return NULL;
    }

    RAD_IsoEntity_t *entity = iso_map_view->map->data[0][y][x].entities[RAD_ISO_UNIT_DEPTH];
    return ((entity != NULL) && (entity->id == unit)) ? entity : NULL;
}

///
/// Wo die Figur der Einheit "unit" auf einem Feld liegt, nach (x, y); false, wenn
/// keines sie hat -- etwa, weil sie gerade zieht.
///
static bool RAD_IsoMapViewFindEntity(const RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit,
                                     int32_t *x, int32_t *y)
{
    for(int32_t ty = 0; ty < RAD_ISO_MAP_SIZE; ++ty)
    {
        for(int32_t tx = 0; tx < RAD_ISO_MAP_SIZE; ++tx)
        {
            if(RAD_IsoMapViewEntityAt(iso_map_view, tx, ty, unit) != NULL)
            {
                *x = tx;
                *y = ty;
                return true;
            }
        }
    }
    return false;
}

///
/// Wie weit der Turm einer Figur auf (x, y) mit Blick "facing" gegen die Wanne
/// gedreht sein muss, damit sein Rohr auf die Mitte von Feld (target_x,
/// target_y) zeigt.
///
static float RAD_IsoMapViewTurretAngleTo(int32_t x, int32_t y, RAD_IsoFacing_t facing,
                                         int32_t target_x, int32_t target_y)
{
    const float toward = RAD_IsoFacingAngleOf((float)(target_x - x), (float)(target_y - y));
    return RAD_IsoAngleNormalize(toward - RAD_IsoFacingAngle(facing));
}

///
/// Wie lange der Turm einer Figur mit dem Bild "sprite" nach dem Schuss auf dem
/// Ziel bleibt: bis die Explosion "explosion" dort durchgelaufen ist
/// (RAD_IsoEffectDurationMs), mindestens aber RAD_IsoUnitFireHoldMs.
///
static float RAD_IsoMapViewFireHoldMs(RAD_IsoUnitSprite_t sprite, RAD_IsoEffectSprite_t explosion)
{
    return fmaxf(RAD_IsoUnitFireHoldMs(sprite), RAD_IsoEffectDurationMs(explosion));
}

///
/// Richtet den Turm der Einheit "unit" auf Feld (target_x, target_y) -- mit
/// "back" nur, bis die Explosion "explosion" vorbei ist
/// (RAD_IsoMapViewFireHoldMs), dann nach vorn zurueck; ohne
/// "has_target" gleich nach vorn.
///
/// Zieht sie gerade, kommt das ans Ende ihres Zuges, gerechnet von dort, wo
/// er endet. Sonst beginnt es jetzt, vom Winkel ihres Turms aus -- ein
/// laufendes Zielen ersetzt es.
///
/// Ohne Figur, ohne drehbaren Turm (RAD_IsoUnitHasTurret), bei einem Ziel auf
/// ihrem eigenen Feld oder ohne freien Platz geschieht nichts.
///
/// Mit "back" feuert sie; dann kommt nach "fire_ms", wann
/// (RAD_UnitAnimationFireMs), und zurueck true. false, wenn sie nicht feuert;
/// dann bleibt "fire_ms" unberuehrt.
///
static bool RAD_IsoMapViewTurnTurret(RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit, bool has_target,
                                     int32_t target_x, int32_t target_y, bool back,
                                     RAD_IsoEffectSprite_t explosion, float *fire_ms)
{
    RAD_IsoMapViewMove_t *move = RAD_IsoMapViewMoveOf(iso_map_view, unit);
    if(move != NULL)
    {
        if(!RAD_IsoUnitHasTurret(move->entity->sprite)
           || (has_target && (move->to_x == target_x) && (move->to_y == target_y)))
        {
            return false;
        }

        const RAD_UnitAnimationState_t end = RAD_UnitAnimationStateAt(move->animation, INFINITY);
        const RAD_IsoEntity_t *entity = move->entity;
        const float target_angle = has_target
                                   ? RAD_IsoMapViewTurretAngleTo(move->to_x, move->to_y, end.facing, target_x, target_y)
                                   : 0.0f;
        if(!RAD_UnitAnimationAppendAim(move->animation, target_angle, back, RAD_IsoMapViewFireHoldMs(entity->sprite, explosion),
                                       RAD_IsoUnitTurretMs(entity->sprite)))
        {
            printf("[IsoMapView] Einheit %d zielt nicht: kein Speicher\n", (int)unit);
            return false;
        }
        return back && RAD_UnitAnimationFireMs(move->animation, fire_ms);
    }

    RAD_IsoMapViewAim_t *aim = RAD_IsoMapViewAimOf(iso_map_view, unit);
    int32_t x = 0, y = 0;
    if(aim != NULL)
    {
        x = aim->x;
        y = aim->y;
    }
    else if(!RAD_IsoMapViewFindEntity(iso_map_view, unit, &x, &y))
    {
        printf("[IsoMapView] Keine Figur fuer Einheit %d zum Zielen\n", (int)unit);
        return false;
    }

    RAD_IsoEntity_t *entity = RAD_IsoMapViewEntityAt(iso_map_view, x, y, unit);
    if((entity == NULL) || !RAD_IsoUnitHasTurret(entity->sprite) || (has_target && (target_x == x) && (target_y == y)))
    {
        return false;
    }

    if(aim == NULL)
    {
        aim = RAD_IsoMapViewFreeAim(iso_map_view);
        if(aim == NULL)
        {
            printf("[IsoMapView] Kein Platz, damit Einheit %d zielt\n", (int)unit);
            return false;
        }
    }
    else
    {
        RAD_DestroyUnitAnimation(&aim->animation);
    }

    const float target_angle = has_target ? RAD_IsoMapViewTurretAngleTo(x, y, entity->facing, target_x, target_y) : 0.0f;
    *aim = (RAD_IsoMapViewAim_t){
        .active = true,
        .unit = unit,
        .x = x,
        .y = y,
        .animation = RAD_CreateUnitAimAnimation((float)x, (float)y, entity->facing, entity->turret_angle,
                                                target_angle, back, RAD_IsoMapViewFireHoldMs(entity->sprite, explosion),
                                                RAD_IsoMapViewNow(), RAD_IsoUnitTurretMs(entity->sprite))
    };
    if(aim->animation == NULL)
    {
        printf("[IsoMapView] Einheit %d zielt nicht: kein Speicher\n", (int)unit);
        RAD_IsoMapViewStopAim(aim);
        return false;
    }
    return back && RAD_UnitAnimationFireMs(aim->animation, fire_ms);
}

///
/// Laesst ab "start_ms" auf Feld (target_x, target_y) die Explosion "sprite"
/// laufen, im Bild fuer die Richtung, aus der der Schuss kommt: von Feld (from_x,
/// from_y), auf eine Blickrichtung gerundet (RAD_IsoFacingOf). Ohne "located"
/// ist der Schuetze nicht zu sehen; dann gilt RAD_ISO_FACING_X_MINUS. Ohne
/// freien Platz explodiert nichts.
///
static void RAD_IsoMapViewExplode(RAD_IsoMapView_t *iso_map_view, RAD_IsoEffectSprite_t sprite, bool located,
                                  int32_t from_x, int32_t from_y, int32_t target_x, int32_t target_y, float start_ms)
{
    RAD_IsoMapViewExplosion_t *explosion = NULL;
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_EXPLOSIONS_MAX; ++i)
    {
        if(!iso_map_view->explosions[i].active)
        {
            explosion = &iso_map_view->explosions[i];
            break;
        }
    }
    if(explosion == NULL)
    {
        printf("[IsoMapView] Kein Platz fuer eine Explosion auf (%d, %d)\n", (int)target_x, (int)target_y);
        return;
    }

    const RAD_IsoFacing_t facing = located
                                   ? RAD_IsoFacingOf((float)(target_x - from_x), (float)(target_y - from_y),
                                                     RAD_ISO_FACING_X_MINUS)
                                   : RAD_ISO_FACING_X_MINUS;
    *explosion = (RAD_IsoMapViewExplosion_t){
        .active = true,
        .shown = false,
        .start_ms = start_ms,
        .effect = {
            .sprite = sprite,
            .facing = facing,
            .x = (float)target_x,
            .y = (float)target_y,
            .layer = 0,
            .progress = 0.0f
        }
    };
}

///
/// Bringt die Explosionen auf "now_ms": eine, deren Schuss gefallen ist, kommt
/// in die Iso-Map, eine, die durchgelaufen ist, wieder heraus.
///
static void RAD_IsoMapViewUpdateExplosions(RAD_IsoMapView_t *iso_map_view, float now_ms)
{
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_EXPLOSIONS_MAX; ++i)
    {
        RAD_IsoMapViewExplosion_t *explosion = &iso_map_view->explosions[i];
        if(!explosion->active || (now_ms < explosion->start_ms))
        {
            continue;
        }

        const float duration_ms = RAD_IsoEffectDurationMs(explosion->effect.sprite);
        const float progress = (duration_ms > 0.0f) ? (now_ms - explosion->start_ms) / duration_ms : 1.0f;
        if(progress >= 1.0f)
        {
            RAD_IsoMapViewStopExplosion(iso_map_view, explosion);
            continue;
        }

        explosion->effect.progress = progress;
        if(!explosion->shown)
        {
            explosion->shown = RAD_IsoMapAddEffect(iso_map_view->map, &explosion->effect);
            if(!explosion->shown)
            {
                printf("[IsoMapView] Iso-Map nimmt keine Explosion mehr\n");
                RAD_IsoMapViewStopExplosion(iso_map_view, explosion);
            }
        }
    }
}

///
/// Beendet eine Explosion und nimmt ihren Effekt aus der Iso-Map.
///
static void RAD_IsoMapViewStopExplosion(RAD_IsoMapView_t *iso_map_view, RAD_IsoMapViewExplosion_t *explosion)
{
    if(explosion->shown)
    {
        RAD_IsoMapRemoveEffect(iso_map_view->map, &explosion->effect);
    }
    *explosion = (RAD_IsoMapViewExplosion_t){ .active = false };
}

void RAD_IsoMapViewAim(RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit, int32_t target_x, int32_t target_y)
{
    printf("[IsoMapView] Einheit %d zielt auf (%d, %d)\n", (int)unit, (int)target_x, (int)target_y);
    RAD_IsoMapViewTurnTurret(iso_map_view, unit, true, target_x, target_y, false, RAD_ISO_EFFECT_SPRITE_NONE, NULL);
}

void RAD_IsoMapViewFire(RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit, int32_t target_x, int32_t target_y,
                        bool explode)
{
    printf("[IsoMapView] Einheit %d feuert auf (%d, %d)%s\n", (int)unit, (int)target_x, (int)target_y,
           explode ? "" : " ohne Treffer");

    // Wo sie steht, bevor der Turm sich dreht: ein Zug, der noch laeuft, endet
    // dort, wo sie feuert.
    int32_t x = 0, y = 0;
    bool located = true;
    const RAD_IsoMapViewMove_t *move = RAD_IsoMapViewMoveOf(iso_map_view, unit);
    const RAD_IsoMapViewAim_t *aim = RAD_IsoMapViewAimOf(iso_map_view, unit);
    if(move != NULL)
    {
        x = move->to_x;
        y = move->to_y;
    }
    else if(aim != NULL)
    {
        x = aim->x;
        y = aim->y;
    }
    else
    {
        located = RAD_IsoMapViewFindEntity(iso_map_view, unit, &x, &y);
    }

    // Welche Explosion, zuerst: so lange bleibt der Turm auf dem Ziel. Ohne
    // keine -- dann haelt er nur die Mindestzeit (RAD_IsoMapViewFireHoldMs).
    const RAD_IsoEffectSprite_t explosion = explode ? RAD_IsoEffectRandomExplosion() : RAD_ISO_EFFECT_SPRITE_NONE;

    float fire_ms = 0.0f;
    if(!RAD_IsoMapViewTurnTurret(iso_map_view, unit, true, target_x, target_y, true, explosion, &fire_ms))
    {
        // Ohne Turm, der sich dreht, feuert sie gleich.
        fire_ms = RAD_IsoMapViewNow();
    }

    if(explode)
    {
        RAD_IsoMapViewExplode(iso_map_view, explosion, located, x, y, target_x, target_y, fire_ms);
    }
}

void RAD_IsoMapViewReleaseAim(RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit)
{
    printf("[IsoMapView] Einheit %d nimmt den Turm zurueck\n", (int)unit);
    RAD_IsoMapViewTurnTurret(iso_map_view, unit, false, 0, 0, false, RAD_ISO_EFFECT_SPRITE_NONE, NULL);
}

///
/// Ein Angriff, wie der Server ihn bestaetigt hat -- gleich, wessen Einheit:
/// sie feuert (RAD_IsoMapViewFire). Hat der Spieler hier schon gezielt, steht
/// der Turm schon; er bleibt kurz und laeuft dann zurueck.
///
/// Explodieren laesst sie das Zielfeld nur, wenn wenigstens ein Schuss getroffen
/// hat -- oder wenn dort niemand stand: dann wird nicht gewuerfelt, gezeigt wird
/// der Angriff trotzdem. Alles Weitere, was er bewirkt hat, steht vorerst nur
/// im Log.
///
static void RAD_IsoMapViewOnUnitAttacked(void *user_argument, const RAD_ClientWorld_t *world,
                                         const RAD_ClientAttackResult_t *result)
{
    (void)world;
    RAD_IsoMapViewLogAttackResult(result);
    const bool explode = (result->hits > 0) || (result->victim == RAD_CLIENT_UNIT_ID_NONE);
    RAD_IsoMapViewFire((RAD_IsoMapView_t*)user_argument, result->attacker, result->target.x, result->target.y, explode);
}

///
/// Schreibt, was ein Angriff bewirkt hat, ins Log: die Zaehler des ganzen
/// Angriffs und darunter eine Zeile je Waffe.
///
static void RAD_IsoMapViewLogAttackResult(const RAD_ClientAttackResult_t *result)
{
    printf("[IsoMapView] Angriff von Einheit %d auf (%d, %d), Ziel %d: %u Schuesse, %u Treffer, %u durch, "
           "%u Lebenspunkte, %u gefallen%s\n",
           (int)result->attacker, (int)result->target.x, (int)result->target.y, (int)result->victim,
           (unsigned)result->shots, (unsigned)result->hits, (unsigned)result->penetrations,
           (unsigned)result->wounds, (unsigned)result->members_killed,
           result->victim_destroyed ? ", Ziel zerstoert" : "");
    for(uint32_t i = 0; i < result->number_of_weapons; ++i)
    {
        const RAD_ClientAttackWeapon_t *weapon = &result->weapons[i];
        printf("[IsoMapView]    Mitglied %u, Waffe %u: %u Schuesse, %u Treffer, %u durch, %u Lebenspunkte, %u gefallen\n",
               (unsigned)weapon->member, (unsigned)weapon->weapon,
               (unsigned)weapon->shots, (unsigned)weapon->hits, (unsigned)weapon->penetrations,
               (unsigned)weapon->wounds, (unsigned)weapon->members_killed);
    }
}

///
/// Ein eigener Angriff, den der Server abgelehnt hat: der Turm, den der Spieler
/// schon aufs Ziel gerichtet hat, laeuft nach vorn zurueck.
///
static void RAD_IsoMapViewOnAttackRejected(void *user_argument, const RAD_ClientWorld_t *world, RAD_ClientUnitId_t unit)
{
    (void)world;
    RAD_IsoMapViewReleaseAim((RAD_IsoMapView_t*)user_argument, unit);
}

///
/// Kommt vor dem "changed" von Start- und Zielfeld (RAD_ClientWorldMoveEntity):
/// die Einheit steht noch auf steps[0], die Iso-Map zeigt sie noch dort.
///
/// Die IsoMapView loest die Entitaet vom Start-Iso-Objekt, haelt sie selbst und
/// traegt sie als Figur ausserhalb eines Feldes in die Iso-Map ein, bis die
/// Animation den Weg abgelaufen ist (RAD_IsoMapViewMove_t). Zieht die Einheit,
/// waehrend ihr voriger Zug noch laeuft, uebernimmt der neue dessen Entitaet --
/// sie bleibt eingetragen und setzt am Start des neuen Weges an, dem Ziel des
/// vorigen.
///
/// Die Figur dreht sich vor jedem Abschnitt, der in eine andere Richtung
/// zeigt, auf der Stelle (view/animations/unit_animation.h) -- auch vor dem
/// ersten. Schaut sie noch nirgendwohin, schaut sie gleich in seine Richtung.
/// Dreht sie sich gerade, schaut sie fuer den neuen Weg in die Richtung, der
/// sie schon naeher ist. Zielt sie gerade, endet das; ist ihr Turm gedreht,
/// laeuft er vor allem anderen nach vorn zurueck.
///
/// Liegt auf dem Start nichts, ist kein Platz frei, die Iso-Map voll oder der
/// Weg zu lang, bleibt alles, wie es war: die Iso-Map folgt dem Zug ueber die
/// beiden "changed", die Figur springt.
///
static void RAD_IsoMapViewOnUnitMoved(void *user_argument, const RAD_ClientWorld_t *world, RAD_ClientUnitId_t unit,
                                      const RAD_ClientPosition_t *steps, size_t count)
{
    (void)world;
    RAD_IsoMapView_t *iso_map_view = (RAD_IsoMapView_t*)user_argument;

    printf("[IsoMapView] Einheit %d \"%s\" zieht, %u Felder:",
           (int)unit, RAD_IsoMapViewUnitName(iso_map_view, unit), (unsigned)count);
    for(size_t i = 0; i < count; ++i)
    {
        printf(" (%d, %d)", (int)steps[i].x, (int)steps[i].y);
    }
    printf("\n");

    if(count > RAD_ISO_MAP_VIEW_MOVE_STEPS_MAX)
    {
        printf("[IsoMapView] Weg zu lang fuer eine Animation\n");
        return;
    }

    const RAD_ClientPosition_t from = steps[0];
    const RAD_ClientPosition_t to = steps[count - 1];

    // Der Turm bleibt, wo er gerade steht; der Zug dreht ihn zurueck.
    RAD_IsoMapViewAim_t *aim = RAD_IsoMapViewAimOf(iso_map_view, unit);
    if(aim != NULL)
    {
        RAD_IsoMapViewStopAim(aim);
    }

    RAD_IsoEntity_t *entity = NULL;
    RAD_IsoMapViewMove_t *move = RAD_IsoMapViewMoveOf(iso_map_view, unit);
    if(move != NULL)
    {
        entity = move->entity;
        RAD_DestroyUnitAnimation(&move->animation);
    }
    else
    {
        move = RAD_IsoMapViewFreeMove(iso_map_view);
        if(move == NULL)
        {
            printf("[IsoMapView] Kein Platz fuer eine weitere Animation\n");
            return;
        }

        // Die Welt hat Start und Ziel schon geprueft; ihr Raster ist das der Iso-Map.
        RAD_IsoObject_t *start = &iso_map_view->map->data[0][from.y][from.x];
        if(start->entities[RAD_ISO_UNIT_DEPTH] == NULL)
        {
            printf("[IsoMapView] Keine Figur auf (%d, %d) fuer eine Animation\n", (int)from.x, (int)from.y);
            return;
        }

        entity = start->entities[RAD_ISO_UNIT_DEPTH];
        entity->x = (float)from.x;
        entity->y = (float)from.y;
        entity->layer = 0;
        if(!RAD_IsoMapAddEntity(iso_map_view->map, entity))
        {
            printf("[IsoMapView] Iso-Map nimmt keine weitere Figur in Bewegung\n");
            return;
        }
        start->entities[RAD_ISO_UNIT_DEPTH] = NULL;
    }

    float xs[RAD_ISO_MAP_VIEW_MOVE_STEPS_MAX];
    float ys[RAD_ISO_MAP_VIEW_MOVE_STEPS_MAX];
    for(size_t i = 0; i < count; ++i)
    {
        xs[i] = (float)steps[i].x;
        ys[i] = (float)steps[i].y;
    }

    if(entity->turning)
    {
        if(entity->turn_progress >= 0.5f)
        {
            entity->facing = entity->turn_to;
        }
        entity->turning = false;
    }
    const RAD_IsoFacing_t facing = entity->facing_set
                                   ? entity->facing
                                   : RAD_IsoFacingOf((count > 1) ? xs[1] - xs[0] : 0.0f,
                                                     (count > 1) ? ys[1] - ys[0] : 0.0f, entity->facing);

    *move = (RAD_IsoMapViewMove_t){
        .active = true,
        .unit = unit,
        .entity = entity,
        .to_x = to.x,
        .to_y = to.y,
        .animation = NULL
    };

    move->animation = RAD_CreateUnitAnimation(xs, ys, count, facing, entity->turret_angle, RAD_IsoMapViewNow(),
                                              RAD_IsoUnitStepMs(entity->sprite), RAD_IsoUnitTurnMs(entity->sprite),
                                              RAD_IsoUnitTurretMs(entity->sprite));
    if(move->animation == NULL)
    {
        // Ohne Animation gleich ans Ziel -- wie ein Zug, der schon abgelaufen ist.
        printf("[IsoMapView] Animation fuer Einheit %d nicht angelegt\n", (int)unit);
        RAD_IsoMapViewFinishMove(iso_map_view, move);
    }
}

///
/// Die Stelle des Besitzers der Einheit "unit" unter den Mitspielern -- der Ring
/// ihrer Figur (rendering/entity.h) --, RAD_ISO_ENTITY_PLAYER_NONE, wenn die Welt
/// die Einheit oder ihren Besitzer nicht kennt.
///
static int32_t RAD_IsoMapViewPlayerOf(const RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit)
{
    const RAD_ClientUnit_t *found = RAD_ClientUnitRepositoryFindConst(RAD_ClientWorldUnits(iso_map_view->world), unit);
    const int32_t player = (found != NULL) ? RAD_ClientWorldPlayerIndex(iso_map_view->world, found->owner) : -1;
    return (player >= 0) ? player : RAD_ISO_ENTITY_PLAYER_NONE;
}

///
/// Traegt das Feld "tile" in die Iso-Map ein (RAD_IsoMapApplyTile), mit dem Ring
/// fuer den Besitzer seiner Einheit.
///
static void RAD_IsoMapViewApplyTile(RAD_IsoMapView_t *iso_map_view, const RAD_ClientTile_t *tile)
{
    const int32_t player = (tile->unit != NULL) ? RAD_IsoMapViewPlayerOf(iso_map_view, tile->unit->id)
                                                : RAD_ISO_ENTITY_PLAYER_NONE;
    RAD_IsoMapApplyTile(iso_map_view->map, tile, player);
}

///
/// Haelt das Zielfeld laufender Zuege frei: RAD_IsoMapApplyTile legt dort eine
/// Entitaet an, sobald die Welt die Einheit darauf meldet -- direkt nach
/// "unit_moved" und bei jeder weiteren Aenderung des Feldes. Solange der Zug
/// laeuft, haelt die IsoMapView die Entitaet aber selbst.
///
static void RAD_IsoMapViewDetachMovingEntities(RAD_IsoMapView_t *iso_map_view, const RAD_ClientTile_t *tile)
{
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_MOVES_MAX; ++i)
    {
        const RAD_IsoMapViewMove_t *move = &iso_map_view->moves[i];
        if(move->active && (move->to_x == tile->x) && (move->to_y == tile->y))
        {
            RAD_IsoObject_t *goal = &iso_map_view->map->data[0][tile->y][tile->x];
            free(goal->entities[RAD_ISO_UNIT_DEPTH]);
            goal->entities[RAD_ISO_UNIT_DEPTH] = NULL;
        }
    }
}

///
/// Richtet eine Figur, die gerade aufs Feld "tile" gekommen ist und noch keine
/// Blickrichtung hat, zum Gegner aus: zur Mitte der Aufstellzone eines anderen
/// Spielers, ohne die zur Mitte der Welt. Eine Figur, die schon schaut -- etwa
/// eine, die am Ende ihres Zuges hier steht --, bleibt, wie sie ist.
///
static void RAD_IsoMapViewFaceEnemy(RAD_IsoMapView_t *iso_map_view, const RAD_ClientTile_t *tile)
{
    RAD_IsoEntity_t *entity = iso_map_view->map->data[0][tile->y][tile->x].entities[RAD_ISO_UNIT_DEPTH];
    if((entity == NULL) || entity->facing_set || (tile->unit == NULL))
    {
        return;
    }

    const RAD_ClientWorld_t *world = iso_map_view->world;
    float target_x = (float)world->width / 2.0f;
    float target_y = (float)world->height / 2.0f;
    for(size_t i = 0; i < RAD_CLIENT_WORLD_PLAYERS; ++i)
    {
        const RAD_ClientDeployZone_t *zone = &world->deploy_zones[i];
        if((zone->owner != RAD_CLIENT_PLAYER_NONE) && (zone->owner != tile->unit->owner))
        {
            target_x = (float)(zone->min_x + zone->max_x) / 2.0f;
            target_y = (float)(zone->min_y + zone->max_y) / 2.0f;
            break;
        }
    }

    entity->facing = RAD_IsoFacingOf(target_x - (float)tile->x, target_y - (float)tile->y, entity->facing);
    entity->facing_set = true;
}

///
/// Beendet einen Zug: die Entitaet kommt aufs Zielfeld zurueck -- wenn die
/// Einheit laut Welt noch dort steht und das Feld gezeichnet wird. Sonst wird
/// sie freigegeben, und die Iso-Map uebernimmt das Feld wieder aus der Welt
/// (RAD_IsoMapApplyTile), etwa wenn die Einheit inzwischen gefallen ist.
///
static void RAD_IsoMapViewFinishMove(RAD_IsoMapView_t *iso_map_view, RAD_IsoMapViewMove_t *move)
{
    const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt(iso_map_view->world, move->to_x, move->to_y);
    RAD_IsoObject_t *goal = &iso_map_view->map->data[0][move->to_y][move->to_x];

    RAD_IsoMapRemoveEntity(iso_map_view->map, move->entity);
    move->entity->turning = false;

    const bool arrived = (tile != NULL) && (tile->unit != NULL) && (tile->unit->id == move->unit)
                         && goal->present && (goal->entities[RAD_ISO_UNIT_DEPTH] == NULL);
    if(arrived)
    {
        goal->entities[RAD_ISO_UNIT_DEPTH] = move->entity;
    }
    else
    {
        free(move->entity);
    }

    printf("[IsoMapView] Einheit %d am Ziel (%d, %d)\n", (int)move->unit, (int)move->to_x, (int)move->to_y);

    if(move->animation != NULL)
    {
        RAD_DestroyUnitAnimation(&move->animation);
    }
    *move = (RAD_IsoMapViewMove_t){ .active = false };

    if(!arrived && (tile != NULL))
    {
        RAD_IsoMapViewApplyTile(iso_map_view, tile);
    }
}

///
/// Die Zeit fuer die Animationen, in ms seit dem Start von SDL. Als float bis
/// etwa 4,6 Stunden Laufzeit auf die Millisekunde genau.
///
static float RAD_IsoMapViewNow(void)
{
    return (float)SDL_GetTicks64();
}

static bool RAD_IsoMapViewObserveWorld(RAD_IsoMapView_t *iso_map_view)
{
    return RAD_ClientWorldSubscribe(iso_map_view->world, (RAD_ClientWorldObserver_t){
        .user_argument = iso_map_view,
        .tile_created = RAD_IsoMapViewOnTileCreated,
        .reserve_created = RAD_IsoMapViewOnReserveCreated,
        .size_changed = RAD_IsoMapViewOnWorldSizeChanged,
        .unit_moved = RAD_IsoMapViewOnUnitMoved,
        .unit_attacked = RAD_IsoMapViewOnUnitAttacked,
        .attack_rejected = RAD_IsoMapViewOnAttackRejected,
        .players_changed = RAD_IsoMapViewOnPlayersChanged
    });
}

///
/// Meldet die IsoMapView an der Welt und an allem in ihr ab, woran sie sich
/// angemeldet haben kann -- wo sie es nicht ist, geschieht nichts.
///
static void RAD_IsoMapViewStopObservingWorld(RAD_IsoMapView_t *iso_map_view)
{
    const RAD_ClientWorld_t *world = iso_map_view->world;

    RAD_ClientWorldUnsubscribe(world, iso_map_view);

    for(int32_t y = 0; y < RAD_ISO_MAP_SIZE; ++y)
    {
        for(int32_t x = 0; x < RAD_ISO_MAP_SIZE; ++x)
        {
            RAD_ClientTileUnsubscribe(&world->tiles[y][x], iso_map_view);
        }
    }

    for(size_t i = 0; i < RAD_CLIENT_WORLD_PLAYERS; ++i)
    {
        RAD_ClientReserveUnsubscribe(&world->reserves[i], iso_map_view);
    }
}

///
/// Legt den Nebel auf die Iso-Map: jedes Feld, das keine eigene Einheit sieht,
/// wird abgedunkelt gezeichnet (RAD_IsoObject_t.light), auf allen Ebenen. Solange
/// der Client seine Spieler-Id nicht kennt, liegt keiner (RAD_ControlVisibility).
///
/// **Die Sicht wandert mit:** eine eigene Einheit, die gerade faehrt, sieht von
/// dem Feld aus, auf dem sie zu sehen ist -- in der Welt steht sie da schon am
/// Ziel. Gerundet auf das naechste Feld.
///
/// **Und blendet weich:** jedes Feld laeuft je Bild ein Stueck auf seine
/// Helligkeit zu, ueber RAD_ISO_MAP_VIEW_FOG_FADE_MS von ganz hell bis in den
/// Nebel. Beim ersten Bild steht sie gleich, wo sie hingehoert.
///
static void RAD_IsoMapViewApplyFog(RAD_IsoMapView_t *iso_map_view, float now_ms)
{
    RAD_ControlUnitPosition_t positions[RAD_ISO_MAP_VIEW_MOVES_MAX];
    size_t number_of_positions = 0;
    for(size_t i = 0; i < RAD_ISO_MAP_VIEW_MOVES_MAX; ++i)
    {
        const RAD_IsoMapViewMove_t *move = &iso_map_view->moves[i];
        if(move->active && (move->entity != NULL))
        {
            positions[number_of_positions++] = (RAD_ControlUnitPosition_t){
                .unit = move->unit,
                .x = (int32_t)floorf(move->entity->x + 0.5f),
                .y = (int32_t)floorf(move->entity->y + 0.5f)
            };
        }
    }

    bool visible[RAD_ISO_MAP_SIZE][RAD_ISO_MAP_SIZE];
    const RAD_ClientPlayerId_t player = (iso_map_view->session != NULL)
        ? (RAD_ClientPlayerId_t)iso_map_view->session->own_player_id
        : RAD_CLIENT_PLAYER_NONE;
    RAD_ControlVisibility(iso_map_view->world, player, positions, number_of_positions, visible);

    // Wie weit in diesem Bild geblendet wird; beim ersten ganz.
    const float full = 255.0f - (float)RAD_ISO_OBJECT_FOG_LIGHT;
    const float step = (iso_map_view->fog_ms < 0.0f)
        ? full
        : full * (now_ms - iso_map_view->fog_ms) / RAD_ISO_MAP_VIEW_FOG_FADE_MS;
    iso_map_view->fog_ms = now_ms;

    for(int32_t y = 0; y < RAD_ISO_MAP_SIZE; ++y)
    {
        for(int32_t x = 0; x < RAD_ISO_MAP_SIZE; ++x)
        {
            const float target = visible[y][x] ? 255.0f : (float)RAD_ISO_OBJECT_FOG_LIGHT;
            float light = iso_map_view->light[y][x];
            light = (light < target) ? fminf(light + step, target) : fmaxf(light - step, target);
            iso_map_view->light[y][x] = light;

            for(int32_t layer = 0; layer < 3; ++layer)
            {
                iso_map_view->map->data[layer][y][x].light = (uint8_t)(light + 0.5f);
            }
        }
    }
}
