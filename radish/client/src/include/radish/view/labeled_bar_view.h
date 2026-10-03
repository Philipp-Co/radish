#ifndef __RAD_VIEW_LABELED_BAR_VIEW_H__
#define __RAD_VIEW_LABELED_BAR_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>

///
/// Ein Text und dahinter ein zweifarbiger Balken, nebeneinander in einer Zeile:
/// links eine TextView (view/text_view.h) ueber 40% der Breite, rechts nach
/// einem kleinen Abstand eine BarView (view/bar_view.h) ueber den Rest. Beide
/// sind so hoch wie die LabeledBarView.
///
/// Die Setter reichen an TextView und BarView weiter; deren Doku gilt.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in view/labeled_bar_view.c.
///
typedef struct RAD_LabeledBarView RAD_LabeledBarView_t;

///
/// Legt eine LabeledBarView mit der oberen linken Ecke bei (x, y) und width x
/// height Pixeln an, samt TextView und BarView; NULL, wenn kein Speicher da ist.
/// "font" reicht sie an die TextView weiter, er gehoert dem Aufrufer.
/// RAD_DestroyLabeledBarView baut alle ab und nullt den Zeiger des Aufrufers.
///
RAD_LabeledBarView_t* RAD_CreateLabeledBarView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font);
void RAD_DestroyLabeledBarView(RAD_LabeledBarView_t **labeled_bar_view);

///
/// RAD_TextViewSetText.
///
bool RAD_LabeledBarViewSetLabel(RAD_LabeledBarView_t *labeled_bar_view, const char *label);

///
/// RAD_BarViewSetValue.
///
void RAD_LabeledBarViewSetValue(RAD_LabeledBarView_t *labeled_bar_view, int32_t current, int32_t maximum);

///
/// RAD_BarViewSetColors.
///
void RAD_LabeledBarViewSetColors(RAD_LabeledBarView_t *labeled_bar_view, SDL_Color filled, SDL_Color remaining);

///
/// Zeichnet die LabeledBarView mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateLabeledBarView(RAD_LabeledBarView_t *labeled_bar_view, SDL_Renderer *renderer);

#endif
