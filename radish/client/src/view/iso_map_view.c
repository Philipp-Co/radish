#include <radish/view/iso_map_view.h>
#include <radish/view/animations/move_animation.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


///
/// Wie viele Zuege sie gleichzeitig selbst darstellt, wie viele Felder ein Weg
/// dafuer hoechstens haben darf, und wie lange ein Schritt von Feld zu Feld
/// dauert. Was darueber hinausgeht, springt wie zuvor.
///
#define RAD_ISO_MAP_VIEW_MOVES_MAX 4
#define RAD_ISO_MAP_VIEW_MOVE_STEPS_MAX 16
#define RAD_ISO_MAP_VIEW_MOVE_STEP_MS 150.0f

///
/// Ein Zug, den die IsoMapView selbst darstellt (RAD_IsoMapViewOnUnitMoved).
///
/// **Die Entitaet ist von ihrem Feld geloest**, solange er laeuft: sie liegt
/// weder auf dem Start- noch auf dem Zielfeld und gehoert der IsoMapView. In der
/// Iso-Map steht sie als Figur ausserhalb eines Feldes (RAD_IsoMapAddEntity),
/// die Animation schreibt ihre Position. Am Ende kommt sie aufs Zielfeld zurueck
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
    /// Schreibt nach entity->x und entity->y.
    ///
    RAD_MoveAnimation_t *animation;
} RAD_IsoMapViewMove_t;

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
    /// Das Iso-Objekt unter dem Mauszeiger (RAD_IsoMapViewHandleMouseMotion), mit
    /// gesetztem "focus" (rendering/iso_object.h); NULL, wenn keines. Gehoert der
    /// Iso-Map.
    ///
    RAD_IsoObject_t *focus_object;

    ///
    /// Das Feld der Welt zu "focus_object" (model/focus.h). Gehoert dem Aufrufer.
    ///
    RAD_ClientFocus_t *focus;
};


static bool RAD_IsoMapViewObserveWorld(RAD_IsoMapView_t *iso_map_view);
static void RAD_IsoMapViewStopObservingWorld(RAD_IsoMapView_t *iso_map_view);
static void RAD_IsoMapViewClearFocus(RAD_IsoMapView_t *iso_map_view);
static void RAD_IsoMapViewDetachMovingEntities(RAD_IsoMapView_t *iso_map_view, const RAD_ClientTile_t *tile);
static void RAD_IsoMapViewFinishMove(RAD_IsoMapView_t *iso_map_view, RAD_IsoMapViewMove_t *move);
static float RAD_IsoMapViewNow(void);


RAD_IsoMapView_t* RAD_CreateIsoMapView(int32_t x, int32_t y, int32_t width, int32_t height, RAD_IsoMap_t *map, const RAD_ClientWorld_t *world, RAD_ClientFocus_t *focus)
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
        .focus = focus
    };

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

        RAD_UpdateMoveAnimation(move->animation, now_ms);

        if(RAD_MoveAnimationIsFinished(move->animation, now_ms))
        {
            RAD_IsoMapViewFinishMove(iso_map_view, move);
        }
    }

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

    RAD_IsoMapApplyTile(iso_map_view->map, tile);
    RAD_IsoMapViewDetachMovingEntities(iso_map_view, tile);
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

    RAD_IsoMapApplyTile(iso_map_view->map, tile);
    RAD_IsoMapViewDetachMovingEntities(iso_map_view, tile);

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

    RAD_IsoEntity_t *entity = NULL;
    RAD_IsoMapViewMove_t *move = RAD_IsoMapViewMoveOf(iso_map_view, unit);
    if(move != NULL)
    {
        entity = move->entity;
        RAD_DestroyMoveAnimation(&move->animation);
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

    *move = (RAD_IsoMapViewMove_t){
        .active = true,
        .unit = unit,
        .entity = entity,
        .to_x = to.x,
        .to_y = to.y,
        .animation = NULL
    };

    move->animation = RAD_CreateMoveAnimation(xs, ys, count, RAD_IsoMapViewNow(), RAD_ISO_MAP_VIEW_MOVE_STEP_MS,
                                              &entity->x, &entity->y);
    if(move->animation == NULL)
    {
        // Ohne Animation gleich ans Ziel -- wie ein Zug, der schon abgelaufen ist.
        printf("[IsoMapView] Animation fuer Einheit %d nicht angelegt\n", (int)unit);
        RAD_IsoMapViewFinishMove(iso_map_view, move);
    }
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
        RAD_DestroyMoveAnimation(&move->animation);
    }
    *move = (RAD_IsoMapViewMove_t){ .active = false };

    if(!arrived && (tile != NULL))
    {
        RAD_IsoMapApplyTile(iso_map_view->map, tile);
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
        .unit_moved = RAD_IsoMapViewOnUnitMoved
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
