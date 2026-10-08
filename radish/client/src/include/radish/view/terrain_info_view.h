#ifndef __RAD_VIEW_TERRAIN_INFO_VIEW_H__
#define __RAD_VIEW_TERRAIN_INFO_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/model/focus.h>

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
/// **Alles gehoert zum Feld im Fokus** (model/focus.h): die TerrainInfoView
/// liest den Fokus bei jedem Update und fragt control/terrain.h nach Name und
/// Werten des Feldes. Steht nichts im Fokus, zeigt sie "-", "MV -" und "CV -".
/// Neu gesetzt werden die Texte nur, wenn sich etwas geaendert hat. Den Fokus
/// schreibt die IsoMapView; er gehoert dem Aufrufer.
///
/// **Ihre Groesse passt sich dem Inhalt an**: sie belegt die linke untere Ecke
/// der Flaeche aus RAD_CreateTerrainInfoView, so hoch wie die drei Zeilen und so
/// breit wie der breiteste Text, je samt Rand -- hoechstens so gross wie diese
/// Flaeche; was dann nicht hineinpasst, wird abgeschnitten. Die Breite folgt
/// jedem neuen Text, die Hoehe steht mit dem Anlegen fest.
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
/// linken Ecke bei (x, y) und width x height Pixeln belegt, mit Name, MV und CV
/// zu "focus"; NULL, wenn kein Speicher da ist. "font" gehoert dem Aufrufer (view/view.h, RAD_ViewFont) und muss die
/// TerrainInfoView ueberleben, ebenso "focus". RAD_DestroyTerrainInfoView
/// nullt den Zeiger des Aufrufers.
///
RAD_TerrainInfoView_t* RAD_CreateTerrainInfoView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font, const RAD_ClientFocus_t *focus);
void RAD_DestroyTerrainInfoView(RAD_TerrainInfoView_t **terrain_info_view);


///
/// Bringt Name, MV und CV auf den Stand des Fokus und zeichnet die TerrainInfoView
/// mit "renderer". Einmal je Frame zu rufen.
///
void RAD_UpdateTerrainInfoView(RAD_TerrainInfoView_t *terrain_info_view, SDL_Renderer *renderer);

#endif
