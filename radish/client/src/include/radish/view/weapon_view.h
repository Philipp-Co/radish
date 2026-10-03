#ifndef __RAD_VIEW_WEAPON_VIEW_H__
#define __RAD_VIEW_WEAPON_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdint.h>

///
/// Die Attribute einer Waffe, je Attribut eine LabeledBarView
/// (view/labeled_bar_view.h), in zwei Zeilen zu je zwei nebeneinander,
/// beschriftet mit ihren Abkuerzungen (view/abbreviations.h):
///
///   ST  Staerke       AP  Durchschlagskraft
///   RG  Reichweite    SH  Anzahl Schuesse
///
/// Jede LabeledBarView nimmt ein Viertel der Flaeche ein, getrennt durch einen
/// kleinen Abstand.
///
/// **Sie kennt keinen Waffen-Typ.** Jedes Attribut hat seinen eigenen Setter;
/// woher die Werte kommen, weiss nur der Aufrufer. Die Werte werden wie bei
/// RAD_BarViewSetValue begrenzt.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in view/weapon_view.c.
///
typedef struct RAD_WeaponView RAD_WeaponView_t;

///
/// Legt eine WeaponView mit der oberen linken Ecke bei (x, y) und width x height
/// Pixeln an, samt ihrer vier LabeledBarViews, alle Werte 0 von 0; NULL, wenn
/// kein Speicher da ist. "font" reicht sie weiter, er gehoert dem Aufrufer.
/// RAD_DestroyWeaponView baut alle ab und nullt den Zeiger des Aufrufers.
///
RAD_WeaponView_t* RAD_CreateWeaponView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font);
void RAD_DestroyWeaponView(RAD_WeaponView_t **weapon_view);

void RAD_WeaponViewSetStrength(RAD_WeaponView_t *weapon_view, int32_t current, int32_t maximum);
void RAD_WeaponViewSetPenetration(RAD_WeaponView_t *weapon_view, int32_t current, int32_t maximum);
void RAD_WeaponViewSetRange(RAD_WeaponView_t *weapon_view, int32_t current, int32_t maximum);
void RAD_WeaponViewSetNumberOfShots(RAD_WeaponView_t *weapon_view, int32_t current, int32_t maximum);

///
/// Zeichnet die WeaponView mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateWeaponView(RAD_WeaponView_t *weapon_view, SDL_Renderer *renderer);

#endif
