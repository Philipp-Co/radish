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
};


static bool RAD_IsoMapViewObserveWorld(RAD_IsoMapView_t *iso_map_view);
static void RAD_IsoMapViewStopObservingWorld(RAD_IsoMapView_t *iso_map_view);


RAD_IsoMapView_t* RAD_CreateIsoMapView(int32_t x, int32_t y, int32_t width, int32_t height, RAD_IsoMap_t *map, const RAD_ClientWorld_t *world)
{
    RAD_IsoMapView_t *iso_map_view = malloc(sizeof(struct RAD_IsoMapView));
    if(iso_map_view == NULL)
    {
        return NULL;
    }

    *iso_map_view = (struct RAD_IsoMapView){
        .area = { .x = x, .y = y, .w = width, .h = height },
        .map = map,
        .world = world
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


//
// Beobachter der Welt (view/iso_map_view.h). "user_argument" ist bei allen die
// IsoMapView selbst. Felder gehen in die Iso-Map, alles andere vorerst nur ins
// Log.
//

static void RAD_IsoMapViewOnTileChanged(void *user_argument, const RAD_ClientTile_t *tile)
{
    RAD_IsoMapView_t *iso_map_view = (RAD_IsoMapView_t*)user_argument;
    printf("[IsoMapView] Feld (%d, %d) geaendert, Typ=%s\n", (int)tile->x, (int)tile->y, RAD_NetTileTypeText(tile->type));

    RAD_IsoMapApplyTile(iso_map_view->map, tile);
}

static void RAD_IsoMapViewOnTileRemoved(void *user_argument, const RAD_ClientTile_t *tile)
{
    RAD_IsoMapView_t *iso_map_view = (RAD_IsoMapView_t*)user_argument;
    printf("[IsoMapView] Feld (%d, %d) verworfen\n", (int)tile->x, (int)tile->y);

    RAD_IsoMapRemoveTile(iso_map_view->map, (uint32_t)tile->x, (uint32_t)tile->y);
}

static void RAD_IsoMapViewOnTileCreated(void *user_argument, const RAD_ClientWorld_t *world, const RAD_ClientTile_t *tile)
{
    (void)world;
    RAD_IsoMapView_t *iso_map_view = (RAD_IsoMapView_t*)user_argument;
    printf("[IsoMapView] Feld (%d, %d) neu, Typ=%s\n", (int)tile->x, (int)tile->y, RAD_NetTileTypeText(tile->type));

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

static void RAD_IsoMapViewOnReserveUnitAdded(void *user_argument, const RAD_ClientReserve_t *reserve, const RAD_ClientUnit_t *unit)
{
    (void)user_argument;
    printf("[IsoMapView] Reserve 0x%llx: Einheit %d \"%s\" neu (%u Einheiten)\n",
           (unsigned long long)reserve->owner, (int)unit->id, unit->name, (unsigned)reserve->number_of_units);
}

static void RAD_IsoMapViewOnReserveUnitChanged(void *user_argument, const RAD_ClientReserve_t *reserve, const RAD_ClientUnit_t *unit)
{
    (void)user_argument;
    printf("[IsoMapView] Reserve 0x%llx: Einheit %d \"%s\" geaendert\n",
           (unsigned long long)reserve->owner, (int)unit->id, unit->name);
}

static void RAD_IsoMapViewOnReserveCreated(void *user_argument, const RAD_ClientWorld_t *world, const RAD_ClientReserve_t *reserve)
{
    (void)world;
    printf("[IsoMapView] Reserve fuer 0x%llx neu\n", (unsigned long long)reserve->owner);

    if(!RAD_ClientReserveSubscribe(reserve, (RAD_ClientReserveObserver_t){
        .user_argument = user_argument,
        .unit_added = RAD_IsoMapViewOnReserveUnitAdded,
        .unit_changed = RAD_IsoMapViewOnReserveUnitChanged
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
