#ifndef __RAD_VIEW_VIEW_H__
#define __RAD_VIEW_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/model/world.h>
#include <radish/rendering/iso_map.h>
#include <radish/view/user_input.h>

///
/// view/ -- die grafischen Komponenten. RAD_RootView_t ist ihre Grundlage, die
/// RootView: sie initialisiert SDL und SDL_ttf, haelt Fenster, Renderer und
/// Schrift, genau eine IsoMapView (view/iso_map_view.h), genau eine
/// TerrainInfoView (view/terrain_info_view.h) und genau eine UnitInfoView
/// (view/unit_info_view.h).
///
/// **Die IsoMapView nimmt die ganze Zeichenflaeche ein**
/// (SDL_GetWindowSizeInPixels). **TerrainInfoView und UnitInfoView liegen am
/// unteren Rand** darueber, die TerrainInfoView links, die UnitInfoView rechts,
/// beide so gross wie ihr Inhalt: die TerrainInfoView hoechstens 200 Pixel breit
/// und 27% der Hoehe der Zeichenflaeche hoch, die UnitInfoView genau 200 Pixel
/// breit und hoechstens so hoch wie die ganze Zeichenflaeche. Ist die
/// Zeichenflaeche schmaler als 400 Pixel, tritt die halbe Breite an die Stelle
/// der 200 Pixel. Die Grenzen stehen mit RAD_CreateView fest -- das Fenster ist
/// nicht veraenderbar.
///
/// **TerrainInfoView und UnitInfoView sind nicht immer sichtbar**, und zwar
/// unabhaengig voneinander (RAD_ViewSetTerrainInfoVisible,
/// RAD_ViewSetUnitInfoVisible). An der Stelle einer unsichtbaren ist die
/// IsoMapView zu sehen.
///
/// **Sie haelt die Kamera-Steuerung** (io/camera_control.h) fuer die Iso-Map
/// und gibt ihr in RAD_UpdateView jedes SDL-Ereignis zuerst: rechte Maustaste
/// und Pfeiltasten verschieben die Kamera. **Das Loslassen der linken
/// Maustaste geht an den User-Input** (view/user_input.h), umgerechnet in das
/// Feld darunter. Alle uebrigen Ereignisse -- etwa die Mausbewegung -- verwirft
/// sie vorerst.
///
/// **Den Frame zeichnet sie selbst**: leeren, die IsoMapView, die sichtbaren
/// von TerrainInfoView und UnitInfoView, anzeigen.
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in view/view.c.
///
typedef struct RAD_RootView RAD_RootView_t;

///
/// Initialisiert SDL und SDL_ttf, oeffnet ein Fenster "title" mit width x height
/// Pixeln samt Renderer, laedt die Schrift "font_path" in "font_size" Punkt,
/// legt ihre Views an, alle sichtbar, und schaltet die Texteingabe ein. Die
/// IsoMapView zeichnet "map", die Kamera-Steuerung verschiebt deren Kamera; die
/// Map gehoert dem Aufrufer und muss die RootView ueberleben. Ebenso
/// "user_input": die RootView ruft ihn nur auf, angelegt und abonniert wird er
/// vom Aufrufer. Und "world": die IsoMapView beobachtet sie
/// (view/iso_map_view.h).
///
/// NULL, wenn einer der Schritte scheitert -- was bis dahin angelegt war, ist
/// dann schon wieder abgebaut. RAD_DestroyView nullt den Zeiger des Aufrufers.
///
RAD_RootView_t* RAD_CreateView(
    const char *title,
    int32_t width,
    int32_t height,
    const char *font_path,
    int32_t font_size,
    RAD_IsoMap_t *map,
    RAD_IoUserInput_t *user_input,
    const RAD_ClientWorld_t *world
);
void RAD_DestroyView(RAD_RootView_t **view);

///
/// Gibt alle anstehenden SDL-Ereignisse an Kamera-Steuerung und User-Input,
/// verwirft den Rest und zeichnet danach den Frame. Einmal je Frame zu rufen.
///
void RAD_UpdateView(RAD_RootView_t *view);

///
/// Setzt (true) oder ruecksetzt (false), ob TerrainInfoView bzw. UnitInfoView
/// sichtbar sind.
///
void RAD_ViewSetTerrainInfoVisible(RAD_RootView_t *view, bool visible);
void RAD_ViewSetUnitInfoVisible(RAD_RootView_t *view, bool visible);

///
/// Was zum Zeichnen gebraucht wird. Gehoert der Komponente: nicht freigeben, und
/// nach RAD_DestroyView nicht mehr benutzen.
///
SDL_Renderer* RAD_ViewRenderer(const RAD_RootView_t *view);
TTF_Font* RAD_ViewFont(const RAD_RootView_t *view);

#endif
