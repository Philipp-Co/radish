#ifndef __RAD_VIEW_TERRAIN_INFO_VIEW_H__
#define __RAD_VIEW_TERRAIN_INFO_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>

///
/// Ein rechteckiger Bereich im Fenster mit einem Terrain, untereinander:
///
///   <Name des Terrains>
///   MV +1       Modifier der Bewegungsreichweite, mit Vorzeichen
///   CV 2        Deckung
///
/// Beschriftet mit den Abkuerzungen aus view/abbreviations.h. Der Hintergrund
/// ist rot.
///
/// **Ihre Groesse passt sich dem Inhalt an**: sie belegt die linke untere Ecke
/// der Flaeche aus RAD_CreateTerrainInfoView, so hoch wie die drei Zeilen und so
/// breit wie der breiteste Text, je samt Rand -- hoechstens so gross wie diese
/// Flaeche; was dann nicht hineinpasst, wird abgeschnitten. Die Breite folgt
/// jedem Setter, die Hoehe steht mit dem Anlegen fest.
///
/// **Sie kennt keinen Terrain-Typ.** Jedes Attribut hat seinen eigenen Setter;
/// woher die Werte kommen, weiss nur der Aufrufer.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in view/terrain_info_view.c.
///
typedef struct RAD_TerrainInfoView RAD_TerrainInfoView_t;

///
/// Legt eine TerrainInfoView an, die hoechstens die Flaeche mit der oberen
/// linken Ecke bei (x, y) und width x height Pixeln belegt, mit leerem Namen und
/// beiden Werten 0; NULL, wenn kein Speicher da ist. "font" gehoert dem Aufrufer (view/view.h, RAD_ViewFont) und muss die
/// TerrainInfoView ueberleben. RAD_DestroyTerrainInfoView nullt den Zeiger des
/// Aufrufers.
///
RAD_TerrainInfoView_t* RAD_CreateTerrainInfoView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font);
void RAD_DestroyTerrainInfoView(RAD_TerrainInfoView_t **terrain_info_view);

///
/// Name des Terrains (RAD_TextViewSetText).
///
bool RAD_TerrainInfoViewSetName(RAD_TerrainInfoView_t *terrain_info_view, const char *name);

///
/// Um wie viele Felder das Terrain die Bewegungsreichweite aendert, negativ wie
/// positiv. Angezeigt als "MV +1", "MV -2", "MV +0". false, wenn fuer den Text
/// kein Speicher da ist (RAD_TextViewSetText) -- wie bei RAD_TerrainInfoViewSetCover.
///
bool RAD_TerrainInfoViewSetMovementModifier(RAD_TerrainInfoView_t *terrain_info_view, int32_t movement_modifier);

///
/// Die Deckung, die das Terrain gibt. Angezeigt als "CV 2".
///
bool RAD_TerrainInfoViewSetCover(RAD_TerrainInfoView_t *terrain_info_view, int32_t cover);

///
/// Zeichnet die TerrainInfoView mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateTerrainInfoView(RAD_TerrainInfoView_t *terrain_info_view, SDL_Renderer *renderer);

#endif
