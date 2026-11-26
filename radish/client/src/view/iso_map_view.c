#include <radish/view/iso_map_view.h>
#include <radish/view/rendering/iso_effect.h>
#include <radish/view/rendering/iso_unit_images.h>
#include <radish/view/rendering/animations/unit_animation.h>
#include <radish/control/visibility.h>
#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>


///
/// Wie viele Felder ein Weg hoechstens haben darf, den sie selbst darstellt.
/// Was darueber hinausgeht, springt wie zuvor. Wie lange ein Schritt von Feld
/// zu Feld und eine Vierteldrehung dauern, sagt das Bild der Figur
/// (RAD_IsoUnitStepMs, RAD_IsoUnitTurnMs).
///
#define RAD_ISO_MAP_VIEW_MOVE_STEPS_MAX 16

///
/// Wie lange das letzte Bild einer zerstoerten Figur stehen bleibt, bevor sie
/// verschwindet.
///
#define RAD_ISO_MAP_VIEW_WRECK_HOLD_MS 1000.0f

///
/// Wie lange ein Feld braucht, um ganz in den Nebel zu fallen oder ganz aus ihm
/// zu kommen (RAD_IsoMapViewApplyFog).
///
#define RAD_ISO_MAP_VIEW_FOG_FADE_MS 180.0f

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
    /// gesetztem "focus" (view/rendering/iso_object.h); NULL, wenn keines. Gehoert der
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
static void RAD_IsoMapViewApplyTile(RAD_IsoMapView_t *iso_map_view, const RAD_ClientTile_t *tile);
static int32_t RAD_IsoMapViewPlayerOf(const RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit);
static void RAD_IsoMapViewFaceEnemy(RAD_IsoMapView_t *iso_map_view, const RAD_ClientTile_t *tile);
static bool RAD_IsoMapViewFinishActivity(RAD_IsoMapView_t *iso_map_view, RAD_IsoEntity_t *entity,
                                         RAD_IsoEntityActivity_t finished);
static void RAD_IsoMapViewUpdateEntities(RAD_IsoMapView_t *iso_map_view, float now_ms);
static void RAD_IsoMapViewApplyFog(RAD_IsoMapView_t *iso_map_view, float now_ms);
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

    // Die Iso-Map ueberlebt die IsoMapView: was die Figuren gerade abspielen,
    // laeuft hier zu Ende, so steht die Karte wieder, wie die Welt es sagt.
    // RAD_IsoMapViewUpdateEntities raeumt dabei auch Figuren weg -- deshalb
    // rueckwaerts, mit INFINITY als "jetzt ist alles vorbei".
    RAD_IsoMapViewUpdateEntities(*iso_map_view, INFINITY);
    RAD_IsoMapUpdateEffects((*iso_map_view)->map, INFINITY);

    RAD_IsoMapViewClearFocus(*iso_map_view);
    free(*iso_map_view);
    *iso_map_view = NULL;
}

void RAD_UpdateIsoMapView(RAD_IsoMapView_t *iso_map_view, SDL_Renderer *renderer)
{
    // Zuerst die Figuren: sie schreiben ihre Position, die RAD_RenderIsoMap
    // gleich zeichnet. Ein fertiger Zug steht auf seinem Zielfeld, noch bevor
    // gezeichnet wird.
    const float now_ms = RAD_IsoMapViewNow();
    RAD_IsoMapViewUpdateEntities(iso_map_view, now_ms);
    RAD_IsoMapUpdateEffects(iso_map_view->map, now_ms);

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
    // Was eine Figur abspielt, sagt sie selbst (RAD_IsoEntityIsBusy): ein Zug,
    // ein Schuss, eine Zerstoerung.
    for(int32_t i = 0; i < RAD_IsoMapEntityCount(iso_map_view->map); ++i)
    {
        if(RAD_IsoEntityIsBusy(RAD_IsoMapEntityAtIndex(iso_map_view->map, i)))
        {
            return true;
        }
    }

    // Und was an Explosionen laeuft oder noch auf seinen Schuss wartet.
    return RAD_IsoMapHasEffects(iso_map_view->map);
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
            }
        }
    }

    // Figuren, die gerade ziehen, stehen auf keinem Feld und kommen oben nicht
    // vor: ihr Ring muss trotzdem stimmen.
    for(int32_t i = 0; i < RAD_IsoMapEntityCount(iso_map_view->map); ++i)
    {
        RAD_IsoEntity_t *entity = RAD_IsoMapEntityAtIndex(iso_map_view->map, i);
        if(RAD_IsoEntityHoldsPosition(entity))
        {
            entity->player = RAD_IsoMapViewPlayerOf(iso_map_view, entity->id);
        }
    }
}

static void RAD_IsoMapViewOnWorldSizeChanged(void *user_argument, const RAD_ClientWorld_t *world)
{
    (void)user_argument;
    printf("[IsoMapView] Weltgroesse %u x %u\n", (unsigned)world->width, (unsigned)world->height);
}

///
/// Wie lange der Turm einer Figur mit dem Bild "sprite" nach dem Schuss auf dem
/// Ziel bleibt: bis die Explosion "explosion" dort durchgelaufen ist
/// (RAD_IsoEffectDurationMs), mindestens aber RAD_IsoUnitAimHoldMs.
///
static float RAD_IsoMapViewFireHoldMs(RAD_IsoUnitSprite_t sprite, RAD_IsoEffectSprite_t explosion)
{
    return fmaxf(RAD_IsoUnitAimHoldMs(sprite), RAD_IsoEffectDurationMs(explosion));
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
    // Aus welcher Richtung der Schuss kommt, weiss nur die View -- sie kennt
    // den Schuetzen. Alles Weitere macht die Iso-Map.
    const RAD_IsoFacing_t facing = located
                                   ? RAD_IsoFacingOf((float)(target_x - from_x), (float)(target_y - from_y),
                                                     RAD_ISO_FACING_X_MINUS)
                                   : RAD_ISO_FACING_X_MINUS;

    if(!RAD_IsoMapStartEffect(iso_map_view->map, sprite, facing, (float)target_x, (float)target_y, start_ms))
    {
        printf("[IsoMapView] Kein Platz fuer eine Explosion auf (%d, %d)\n", (int)target_x, (int)target_y);
    }
}

void RAD_IsoMapViewAim(RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit, int32_t target_x, int32_t target_y)
{
    printf("[IsoMapView] Einheit %d zielt auf (%d, %d)\n", (int)unit, (int)target_x, (int)target_y);

    RAD_IsoEntity_t *entity = RAD_IsoMapEntityOf(iso_map_view->map, unit);
    if(entity != NULL)
    {
        RAD_IsoEntityAim(entity, true, target_x, target_y, false,
                         RAD_IsoMapViewFireHoldMs(entity->sprite, RAD_ISO_EFFECT_SPRITE_NONE),
                         RAD_IsoMapViewNow(), NULL);
    }
}

void RAD_IsoMapViewFire(RAD_IsoMapView_t *iso_map_view, RAD_ClientUnitId_t unit, int32_t target_x, int32_t target_y,
                        bool explode)
{
    printf("[IsoMapView] Einheit %d feuert auf (%d, %d)%s\n", (int)unit, (int)target_x, (int)target_y,
           explode ? "" : " ohne Treffer");

    // Wo sie steht, bevor der Turm sich dreht: ein Zug, der noch laeuft, endet
    // dort, wo sie feuert (RAD_IsoEntityRestingPlace).
    int32_t x = 0, y = 0;
    RAD_IsoEntity_t *entity = RAD_IsoMapEntityOf(iso_map_view->map, unit);
    const bool located = (entity != NULL);
    if(located)
    {
        RAD_IsoEntityRestingPlace(entity, &x, &y);
    }

    // Welche Explosion, zuerst: so lange bleibt der Turm auf dem Ziel. Ohne
    // keine -- dann haelt er nur die Mindestzeit (RAD_IsoMapViewFireHoldMs).
    const RAD_IsoEffectSprite_t explosion = explode ? RAD_IsoEffectRandomExplosion() : RAD_ISO_EFFECT_SPRITE_NONE;

    float fire_ms = 0.0f;
    if((entity == NULL)
       || !RAD_IsoEntityAim(entity, true, target_x, target_y, true,
                            RAD_IsoMapViewFireHoldMs(entity->sprite, explosion),
                            RAD_IsoMapViewNow(), &fire_ms))
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

    RAD_IsoEntity_t *entity = RAD_IsoMapEntityOf(iso_map_view->map, unit);
    if(entity != NULL)
    {
        RAD_IsoEntityAim(entity, false, 0, 0, false,
                         RAD_IsoMapViewFireHoldMs(entity->sprite, RAD_ISO_EFFECT_SPRITE_NONE),
                         RAD_IsoMapViewNow(), NULL);
    }
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
/// Die IsoMapView gibt der Figur den Weg (RAD_IsoEntityMoveAlong); von da an
/// bestimmt die Figur selbst, wo sie steht, bis sie durch ist. Zieht die
/// Einheit, waehrend ihr voriger Zug noch laeuft, ersetzt der neue ihn und
/// setzt dort an, wo sie gerade steht.
///
/// Die Figur dreht sich vor jedem Abschnitt, der in eine andere Richtung
/// zeigt, auf der Stelle (view/rendering/animations/unit_animation.h) -- auch vor dem
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

    const RAD_ClientPosition_t to = steps[count - 1];

    // Die Welt hat Start und Ziel schon geprueft; ihr Raster ist das der
    // Iso-Map. Ein laufender Zug und ein Zielen enden damit -- darum kuemmert
    // sich die Figur (RAD_IsoEntityMoveAlong).
    RAD_IsoEntity_t *entity = RAD_IsoMapEntityOf(iso_map_view->map, unit);
    if(entity == NULL)
    {
        printf("[IsoMapView] Keine Figur fuer Einheit %d, die zieht\n", (int)unit);
        return;
    }

    float xs[RAD_ISO_MAP_VIEW_MOVE_STEPS_MAX];
    float ys[RAD_ISO_MAP_VIEW_MOVE_STEPS_MAX];
    for(size_t i = 0; i < count; ++i)
    {
        xs[i] = (float)steps[i].x;
        ys[i] = (float)steps[i].y;
    }

    if(!RAD_IsoEntityMoveAlong(entity, xs, ys, count, to.x, to.y, RAD_IsoMapViewNow()))
    {
        // Ohne Animation steht sie gleich am Ziel -- wie ein Zug, der schon
        // abgelaufen ist.
        printf("[IsoMapView] Animation fuer Einheit %d nicht angelegt\n", (int)unit);
        RAD_IsoMapViewFinishActivity(iso_map_view, entity, RAD_ISO_ENTITY_MOVING);
    }
}

///
/// Die Stelle des Besitzers der Einheit "unit" unter den Mitspielern -- der Ring
/// ihrer Figur (view/rendering/entity.h) --, RAD_ISO_ENTITY_PLAYER_NONE, wenn die Welt
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
/// Richtet eine Figur, die gerade aufs Feld "tile" gekommen ist und noch keine
/// Blickrichtung hat, zum Gegner aus: zur Mitte der Aufstellzone eines anderen
/// Spielers, ohne die zur Mitte der Welt. Eine Figur, die schon schaut -- etwa
/// eine, die am Ende ihres Zuges hier steht --, bleibt, wie sie ist.
///
static void RAD_IsoMapViewFaceEnemy(RAD_IsoMapView_t *iso_map_view, const RAD_ClientTile_t *tile)
{
    RAD_IsoEntity_t *entity = RAD_IsoMapEntityAt(iso_map_view->map, tile->x, tile->y);
    if((entity == NULL) || RAD_IsoEntityHoldsPosition(entity) || entity->facing_set || (tile->unit == NULL))
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
/// Was eine Figur gerade zu Ende gespielt hat ("finished",
/// RAD_IsoEntityUpdate), gegen die Welt gehalten -- das kann nur die IsoMapView,
/// die Figur kennt die Welt nicht.
///
/// Nach einem Zug bleibt sie auf ihrem Zielfeld, wenn ihre Einheit laut Welt
/// noch dort steht und das Feld gezeichnet wird. Sonst kommt sie weg, und die
/// Iso-Map uebernimmt das Feld wieder aus der Welt (RAD_IsoMapApplyTile), etwa
/// wenn die Einheit inzwischen gefallen ist. Ein Wrack, das durch ist, kommt
/// immer weg.
///
/// true, wenn die Figur noch da ist; false, wenn sie aus der Map genommen wurde --
/// dann zeigt kein Zeiger mehr auf sie.
///
static bool RAD_IsoMapViewFinishActivity(RAD_IsoMapView_t *iso_map_view, RAD_IsoEntity_t *entity,
                                         RAD_IsoEntityActivity_t finished)
{
    if(finished == RAD_ISO_ENTITY_WRECK)
    {
        RAD_IsoMapRemoveEntity(iso_map_view->map, entity->id);
        return false;
    }

    if(finished != RAD_ISO_ENTITY_MOVING)
    {
        return true;
    }

    // Der Zug ist vorbei: die Figur steht auf ihrem Zielfeld
    // (RAD_IsoEntityUpdate).
    const RAD_ClientUnitId_t unit = entity->id;
    int32_t to_x = 0, to_y = 0;
    RAD_IsoEntityRestingPlace(entity, &to_x, &to_y);

    const RAD_ClientTile_t *tile = RAD_ClientWorldTileAt(iso_map_view->world, to_x, to_y);
    const RAD_IsoObject_t *goal = &iso_map_view->map->data[to_y][to_x];

    const bool arrived = (tile != NULL) && (tile->unit != NULL) && (tile->unit->id == unit) && goal->present;
    printf("[IsoMapView] Einheit %d am Ziel (%d, %d)\n", (int)unit, (int)to_x, (int)to_y);

    if(arrived)
    {
        entity->x = (float)to_x;
        entity->y = (float)to_y;
        return true;
    }

    RAD_IsoMapRemoveEntity(iso_map_view->map, unit);
    if(tile != NULL)
    {
        RAD_IsoMapViewApplyTile(iso_map_view, tile);
    }
    return false;
}

///
/// Bringt alle Figuren der Iso-Map auf "now_ms" (RAD_IsoEntityUpdate) und haelt
/// gegen die Welt, was dabei zu Ende geht. Eine Figur, die dabei verschwindet,
/// nimmt ihren Platz in der Ablage mit -- deshalb zaehlt "i" dann nicht weiter.
///
static void RAD_IsoMapViewUpdateEntities(RAD_IsoMapView_t *iso_map_view, float now_ms)
{
    for(int32_t i = 0; i < RAD_IsoMapEntityCount(iso_map_view->map); )
    {
        RAD_IsoEntity_t *entity = RAD_IsoMapEntityAtIndex(iso_map_view->map, i);
        const RAD_IsoEntityActivity_t finished = RAD_IsoEntityUpdate(entity, now_ms);
        if((finished == RAD_ISO_ENTITY_IDLE) || RAD_IsoMapViewFinishActivity(iso_map_view, entity, finished))
        {
            ++i;
        }
    }
}

///
/// Kommt vor dem "changed" des Feldes (RAD_ClientWorldRemoveUnit): die Einheit
/// steht noch auf "position", die Iso-Map zeigt sie noch dort -- oder ihr Zug
/// laeuft noch.
///
/// Die IsoMapView laesst die Figur ihre Zerstoerung zeigen
/// (RAD_IsoEntityDestroy, RAD_ISO_UNIT_ANIMATION_DESTROYED), beginnend mit dem
/// Schuss, der sie getroffen hat (RAD_IsoMapImpactMs) -- bis dahin sieht sie
/// aus wie zuvor. Ein laufender Zug endet damit.
/// Nach der Folge bleibt ihr letztes Bild RAD_ISO_MAP_VIEW_WRECK_HOLD_MS stehen,
/// dann verschwindet sie.
///
/// Ohne geladenes Bild verschwindet sie sofort mit dem Feld, wie frueher.
///
static void RAD_IsoMapViewOnUnitDestroyed(void *user_argument, const RAD_ClientWorld_t *world, RAD_ClientUnitId_t unit,
                                          RAD_ClientPosition_t position)
{
    (void)world;
    RAD_IsoMapView_t *iso_map_view = (RAD_IsoMapView_t*)user_argument;

    // Die Figur steht da, wo die Welt die Einheit zuletzt sah -- oder sie zieht
    // noch; dann endet ihr Zug hier (RAD_IsoEntityDestroy).
    RAD_IsoEntity_t *entity = RAD_IsoMapEntityOf(iso_map_view->map, unit);
    if((entity == NULL) || !RAD_IsoUnitCanAim(entity->sprite))
    {
        return;
    }

    // Der Einschlag, der sie getroffen hat: mit ihm beginnt die Folge, und
    // solange bleibt sie heil stehen.
    const float start_ms = RAD_IsoMapImpactMs(iso_map_view->map, position.x, position.y, RAD_IsoMapViewNow());
    const float end_ms = start_ms
                         + RAD_IsoUnitSequenceMsOf(entity->sprite, RAD_ISO_UNIT_ANIMATION_DESTROYED, entity->facing)
                         + RAD_ISO_MAP_VIEW_WRECK_HOLD_MS;
    RAD_IsoEntityDestroy(entity, start_ms, end_ms);
    printf("[IsoMapView] Einheit %d wird zerstoert auf (%d, %d)\n", (int)unit, (int)position.x, (int)position.y);
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
        .players_changed = RAD_IsoMapViewOnPlayersChanged,
        .unit_destroyed = RAD_IsoMapViewOnUnitDestroyed
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
    RAD_ControlUnitPosition_t positions[RAD_ISO_MAP_ENTITIES_MAX];
    size_t number_of_positions = 0;
    for(int32_t i = 0; i < RAD_IsoMapEntityCount(iso_map_view->map); ++i)
    {
        const RAD_IsoEntity_t *entity = RAD_IsoMapEntityAtIndex(iso_map_view->map, i);
        if(entity->activity == RAD_ISO_ENTITY_MOVING)
        {
            positions[number_of_positions++] = (RAD_ControlUnitPosition_t){
                .unit = entity->id,
                .x = (int32_t)floorf(entity->x + 0.5f),
                .y = (int32_t)floorf(entity->y + 0.5f)
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

            iso_map_view->map->data[y][x].light = (uint8_t)(light + 0.5f);
        }
    }
}
