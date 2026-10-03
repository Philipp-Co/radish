#ifndef __RAD_VIEW_ENTITY_VIEW_H__
#define __RAD_VIEW_ENTITY_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

///
/// Eine Entitaet einer Einheit mit ihren Waffen, untereinander:
///
///   <Name der Entitaet>                     TextView
///   LP  [#####--]      AR  [###--]          LabeledBarView
///   ST  [##---]        RS  [####-]          je Attribut
///   <Name der Waffe>                        TextView        \  je Waffe,
///   ST  [###--]        AP  [#--]            WeaponView      |  hoechstens
///   RG  [#-]           SH  [####]                           /  RAD_ENTITY_VIEW_WEAPONS_MAX
///
/// Beschriftet wird mit den Abkuerzungen aus view/abbreviations.h.
///
/// Jede Zeile ist eine Zeile der Schrift hoch (TTF_FontLineSkip), getrennt durch
/// einen kleinen Abstand. Eine Waffe, die nicht mehr ganz in die Flaeche passt,
/// wird nicht gezeichnet.
///
/// **Sie kennt keinen Entitaeten-Typ.** Jedes Attribut hat seinen eigenen Setter;
/// woher die Werte kommen, weiss nur der Aufrufer. Die Setter reichen an
/// TextView, LabeledBarView und WeaponView weiter; deren Doku gilt.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Hoechstzahl der Waffen einer Entitaet.
///
#define RAD_ENTITY_VIEW_WEAPONS_MAX 4

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in view/entity_view.c.
///
typedef struct RAD_EntityView RAD_EntityView_t;

///
/// Legt eine EntityView mit der oberen linken Ecke bei (x, y) und width x height
/// Pixeln an, mit leerem Namen, allen Attributen 0 von 0 und ohne Waffen; NULL,
/// wenn kein Speicher da ist. "font" reicht sie weiter, er gehoert dem
/// Aufrufer. RAD_DestroyEntityView baut alles ab und nullt den Zeiger des
/// Aufrufers.
///
RAD_EntityView_t* RAD_CreateEntityView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font);
void RAD_DestroyEntityView(RAD_EntityView_t **entity_view);

///
/// Wie hoch eine EntityView mit "number_of_weapons" Waffen in "font" sein muss,
/// damit alle ganz hineinpassen, in Pixeln. "number_of_weapons" wird auf
/// RAD_ENTITY_VIEW_WEAPONS_MAX begrenzt.
///
int32_t RAD_EntityViewHeight(TTF_Font *font, size_t number_of_weapons);

bool RAD_EntityViewSetName(RAD_EntityView_t *entity_view, const char *name);

///
/// Attribute der Entitaet, je "current" von "maximum" (RAD_BarViewSetValue):
/// Lebenspunkte (LP), Ruestung (AR), Staerke (ST) und Widerstand (RS).
///
void RAD_EntityViewSetHitPoints(RAD_EntityView_t *entity_view, int32_t current, int32_t maximum);
void RAD_EntityViewSetArmor(RAD_EntityView_t *entity_view, int32_t current, int32_t maximum);
void RAD_EntityViewSetStrength(RAD_EntityView_t *entity_view, int32_t current, int32_t maximum);
void RAD_EntityViewSetResistance(RAD_EntityView_t *entity_view, int32_t current, int32_t maximum);

///
/// Wie viele Waffen die Entitaet hat, hoechstens RAD_ENTITY_VIEW_WEAPONS_MAX.
/// Neu hinzukommende beginnen mit leerem Namen und allen Werten 0 von 0; was
/// ueber die neue Zahl hinausgeht, wird vergessen. false, wenn
/// "number_of_weapons" zu gross ist -- dann bleibt alles, wie es war.
///
bool RAD_EntityViewSetNumberOfWeapons(RAD_EntityView_t *entity_view, size_t number_of_weapons);

///
/// Attribute der Waffe "weapon". false, wenn "weapon" nicht unter der Zahl aus
/// RAD_EntityViewSetNumberOfWeapons liegt -- dann bleibt alles, wie es war.
///
bool RAD_EntityViewSetWeaponName(RAD_EntityView_t *entity_view, size_t weapon, const char *name);
bool RAD_EntityViewSetWeaponStrength(RAD_EntityView_t *entity_view, size_t weapon, int32_t current, int32_t maximum);
bool RAD_EntityViewSetWeaponPenetration(RAD_EntityView_t *entity_view, size_t weapon, int32_t current, int32_t maximum);
bool RAD_EntityViewSetWeaponRange(RAD_EntityView_t *entity_view, size_t weapon, int32_t current, int32_t maximum);
bool RAD_EntityViewSetWeaponNumberOfShots(RAD_EntityView_t *entity_view, size_t weapon, int32_t current, int32_t maximum);

///
/// Zeichnet die EntityView mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateEntityView(RAD_EntityView_t *entity_view, SDL_Renderer *renderer);

#endif
