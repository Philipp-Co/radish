#include <radish/view/iso_map_view.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_IsoMapView
{
    SDL_Rect area;
    RAD_IsoMap_t *map;
    const RAD_ClientWorld_t *world;

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
    RAD_IsoMapViewClearFocus(*iso_map_view);
    free(*iso_map_view);
    *iso_map_view = NULL;
}

void RAD_UpdateIsoMapView(RAD_IsoMapView_t *iso_map_view, SDL_Renderer *renderer)
{
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

static bool RAD_IsoMapViewObserveWorld(RAD_IsoMapView_t *iso_map_view)
{
    return RAD_ClientWorldSubscribe(iso_map_view->world, (RAD_ClientWorldObserver_t){
        .user_argument = iso_map_view,
        .tile_created = RAD_IsoMapViewOnTileCreated,
        .reserve_created = RAD_IsoMapViewOnReserveCreated,
        .size_changed = RAD_IsoMapViewOnWorldSizeChanged
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
