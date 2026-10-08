#ifndef __RAD_VIEW_ISO_MAP_VIEW_H__
#define __RAD_VIEW_ISO_MAP_VIEW_H__

#include <SDL2/SDL.h>
#include <stdint.h>
#include <radish/model/focus.h>
#include <radish/model/world.h>
#include <radish/rendering/iso_map.h>

///
/// Ein rechteckiger Bereich im Fenster mit der Iso-Map (rendering/iso_map.h):
/// ein schwarzer Hintergrund, darueber die Karte, wie RAD_RenderIsoMap sie
/// zeichnet. Was ueber die Flaeche hinausgeht, wird abgeschnitten.
///
/// **Die Iso-Map gehoert ihr nicht.** Sie bekommt sie bei RAD_CreateIsoMapView;
/// bewegt wird deren Kamera von aussen (io/camera_control.h).
///
/// **Gefuellt wird die Iso-Map aus dem Model.** Die IsoMapView beobachtet die
/// Welt (model/world.h, model/events/observable.h) und traegt jedes neue und
/// jedes geaenderte Feld in die Iso-Map ein (RAD_IsoMapApplyTile), ein
/// verworfenes nimmt sie heraus. Ein bestaetigter Zug kommt als "changed" der
/// beiden Felder an. Weltgroesse und Reserven schreibt sie vorerst nur ins Log.
///
/// An jedem neuen Feld und jeder neuen Reserve meldet sie sich selbst an;
/// RAD_DestroyIsoMapView meldet sie ueberall wieder ab.
///
/// **RAD_RenderIsoMap kennt die Flaeche nicht**: sie zeichnet in
/// Fensterkoordinaten, verschoben nur um die Kamera. Liegt die Flaeche nicht bei
/// (0, 0), verschiebt sich die Karte also nicht mit.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in view/iso_map_view.c.
///
typedef struct RAD_IsoMapView RAD_IsoMapView_t;

///
/// Legt eine IsoMapView mit der oberen linken Ecke bei (x, y) und width x height
/// Pixeln an, die "map" zeichnet, "world" beobachtet und "focus" schreibt
/// (RAD_IsoMapViewHandleMouseMotion); NULL, wenn kein Speicher da ist oder die
/// Welt keinen Beobachter mehr annimmt. "map", "world" und "focus" duerfen nicht
/// NULL sein und muessen die IsoMapView ueberleben. RAD_DestroyIsoMapView laesst
/// alle drei stehen -- "focus" auf nichts gesetzt -- und nullt den Zeiger des
/// Aufrufers.
///
/// Am besten, bevor etwas in der Welt steht -- sonst entgeht ihr, was schon da
/// ist.
///
RAD_IsoMapView_t* RAD_CreateIsoMapView(int32_t x, int32_t y, int32_t width, int32_t height, RAD_IsoMap_t *map, const RAD_ClientWorld_t *world, RAD_ClientFocus_t *focus);
void RAD_DestroyIsoMapView(RAD_IsoMapView_t **iso_map_view);

///
/// Zeichnet die IsoMapView mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateIsoMapView(RAD_IsoMapView_t *iso_map_view, SDL_Renderer *renderer);

///
/// Wertet eine Mausbewegung aus (view/view.h). Das Iso-Objekt unter dem Zeiger
/// wird zum Fokus der IsoMapView: sein "focus" wird gesetzt, es wird also
/// hervorgehoben gezeichnet (rendering/iso_object.h), das des vorigen
/// zurueckgesetzt. Das Feld der Welt dazu kommt in "focus" (model/focus.h). Ein
/// Wechsel geht zusaetzlich ins Log. Liegt der Zeiger ausserhalb ihrer Flaeche,
/// des Rasters oder ueber einem Feld, das noch nicht gezeichnet wird, hat sie
/// keinen Fokus. Verwirft die Welt das Feld im Fokus, steht danach nichts mehr
/// darin.
///
void RAD_IsoMapViewHandleMouseMotion(RAD_IsoMapView_t *iso_map_view, const SDL_MouseMotionEvent *motion);

#endif
