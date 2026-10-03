#ifndef __RAD_VIEW_BAR_VIEW_H__
#define __RAD_VIEW_BAR_VIEW_H__

#include <SDL2/SDL.h>
#include <stdint.h>

///
/// Ein zweifarbiger Balken: von links der gefuellte Anteil current/maximum in
/// der einen Farbe, der Rest bis zum rechten Rand in der anderen. Was er
/// darstellt -- Lebenspunkte, Fortschritt --, weiss nur der Aufrufer.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in view/bar_view.c.
///
typedef struct RAD_BarView RAD_BarView_t;

///
/// Legt eine BarView mit der oberen linken Ecke bei (x, y) und width x height
/// Pixeln an, leer (0 von 0), gefuellt in Gruen und der Rest in Rot; NULL, wenn
/// kein Speicher da ist. RAD_DestroyBarView nullt den Zeiger des Aufrufers.
///
RAD_BarView_t* RAD_CreateBarView(int32_t x, int32_t y, int32_t width, int32_t height);
void RAD_DestroyBarView(RAD_BarView_t **bar_view);

///
/// Wie weit der Balken gefuellt ist: "current" von "maximum". "current" wird auf
/// 0 bis "maximum" begrenzt; ist "maximum" nicht groesser als 0, ist nichts
/// gefuellt.
///
void RAD_BarViewSetValue(RAD_BarView_t *bar_view, int32_t current, int32_t maximum);

///
/// Farbe des gefuellten Anteils und des Rests.
///
void RAD_BarViewSetColors(RAD_BarView_t *bar_view, SDL_Color filled, SDL_Color remaining);

///
/// Zeichnet die BarView mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateBarView(RAD_BarView_t *bar_view, SDL_Renderer *renderer);

#endif
