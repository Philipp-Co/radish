#ifndef __RAD_VIEW_VIEW_H__
#define __RAD_VIEW_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/model/focus.h>
#include <radish/model/world.h>
#include <radish/rendering/iso_map.h>
#include <radish/io/net_session.h>

///
/// view/ -- die grafischen Komponenten. RAD_RootView_t ist ihre Grundlage, die
/// RootView: sie initialisiert SDL und SDL_ttf, haelt Fenster, Renderer und
/// Schrift, genau eine IsoMapView (view/iso_map_view.h), genau eine
/// TerrainInfoView (view/terrain_info_view.h), genau eine UnitInfoView
/// (view/unit_info_view.h), genau eine ContextMenuView
/// (view/context_menu/context_menu_view.h) -- samt deren UnitDeploymentView --, genau
/// eine EndTurnView (view/end_turn_view.h) und genau eine DebugView
/// (view/debug/debug_view.h).
///
/// **Die DebugView liegt in der linken oberen Ecke**, 200 Pixel breit
/// (hoechstens so breit wie die Zeichenflaeche), so hoch wie ihr Inhalt, und
/// ueber allen anderen Views. Nur die RootView greift auf sie zu: sie misst die
/// Frames je Sekunde und die Dauer jedes RAD_UpdateView selbst und gibt ihr
/// einmal je Sekunde die Durchschnitte ("avg fps", "avg update"); die laengste
/// Dauer ("max update") gleich, wenn sie steigt -- seit 5 Sekunden nach dem
/// Anlegen, die langsamen ersten Frames zaehlen nicht mit.
///
/// **Die ContextMenuView bekommt jeden Linksklick auf die Karte** als
/// Feldauswahl (RAD_ContextMenuViewOnTileSelected); ob sie sich oeffnet oder
/// schliesst, entscheidet ihre Zustandsmaschine
/// (view/context_menu/context_menu_state_machine.h). Oeffnet sie sich, setzt
/// die RootView sie mittig ueber das angeklickte Feld, samt Kamera. Sie liegt
/// ueber allen anderen Views; anfangs ist sie geschlossen. Ihr Ring ist
/// durchscheinend, damit die Karte darunter zu sehen bleibt.
///
/// **Ihre Eintraege haengen vom angeklickten Feld ab**: unten links immer
/// einer zum Schliessen; oben rechts auf einem freien Feld einer zum
/// Deployen einer Einheit aus der Reserve, auf einem Feld mit einer eigenen
/// Einheit zwei, Move und Attack, mit einer fremden keiner. Der zum Schliessen blendet sie aus. Der zum Deployen
/// oeffnet unter der Maus ihre UnitDeploymentView, 200 Pixel breit
/// (hoechstens so breit wie die Zeichenflaeche) und ein Drittel so hoch, mit
/// den Einheiten in der Reserve des eigenen Spielers; die holt die RootView
/// bei jedem Oeffnen der ContextMenuView aus der Welt
/// (RAD_ClientWorldReserve). Ist dort eine Einheit gewaehlt, bestaetigt ein
/// zweiter Klick auf Deploy sie, und das Kommando geht an den Server
/// (control/deploy_unit.h). Move auf einer eigenen Einheit blendet sie aus:
/// jeder Linksklick auf die Karte ist dann ein Schritt auf dem Weg der
/// Einheit, soweit ihre Reichweite die Kosten der Felder traegt -- ein Klick
/// auf die Einheit selbst bricht ab und oeffnet sie dort wieder --, ein
/// zweiter Klick auf das letzte Feld schliesst ihn ab, und sie
/// oeffnet sich ueber diesem Feld; ein Klick auf Move schickt den Zug an den
/// Server (control/move_unit.h), Schliessen verwirft ihn. Attack auf einer
/// eigenen Einheit blendet sie ebenso aus: der naechste Linksklick auf ein
/// Feld in Reichweite ihrer Waffen ist das Ziel -- ein Klick auf die Einheit
/// selbst bricht ab und oeffnet sie dort wieder --, sie oeffnet sich darueber,
/// und ein Klick auf Attack schickt den Angriff an den Server
/// (control/attack_unit.h).
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
/// **Die EndTurnView liegt unten mittig** darueber, zwischen den beiden, 160
/// Pixel breit (hoechstens so breit wie die Zeichenflaeche) und 32 hoch. Sie ist
/// immer sichtbar: ein Knopf zum Abgeben des Zuges, der zeigt, wer dran ist.
///
/// **TerrainInfoView und UnitInfoView sind nicht immer sichtbar**, und zwar
/// unabhaengig voneinander (RAD_ViewSetTerrainInfoVisible,
/// RAD_ViewSetUnitInfoVisible). An der Stelle einer unsichtbaren ist die
/// IsoMapView zu sehen.
///
/// **Die UnitInfoView erscheint zudem nur, wenn ein Feld mit Einheit
/// ausgewaehlt ist**: in der ContextMenuView ist ein Feld ausgewaehlt, auf dem
/// eine Einheit steht -- auch, waehrend ihr Weg zusammengeklickt wird. Schliesst sie sich, oder steht dort keine (mehr), ist die
/// UnitInfoView verschwunden -- auch wenn sie eingeschaltet ist. **Sie zeigt
/// die Einheit auf diesem Feld** und folgt ihr, solange sie dort steht: die
/// RootView beobachtet sie (model/unit.h) und fuellt die UnitInfoView neu, wenn
/// sie sich aendert.
///
/// **Sie haelt die Kamera-Steuerung** (io/camera_control.h) fuer die Iso-Map
/// und gibt ihr in RAD_UpdateView jedes SDL-Ereignis zuerst: rechte Maustaste
/// und Pfeiltasten verschieben die Kamera. **Danach gehen Maustasten an die
/// ContextMenuView**, solange sie zu sehen ist (RAD_ContextMenuViewHandleMouseButton);
/// trifft eine einen ihrer Eintraege, ist sie damit erledigt. Danach an die
/// EndTurnView (RAD_EndTurnViewHandleMouseButton); was ihre Flaeche trifft,
/// ist damit ebenso erledigt. **Das Loslassen
/// der linken Maustaste waehlt sonst das Feld darunter aus** -- fuer die
/// ContextMenuView, siehe oben. **Die Mausbewegung geht an
/// die ContextMenuView**, solange sie zu sehen ist, **und an die IsoMapView**
/// (RAD_IsoMapViewHandleMouseMotion), auch waehrend die Kamera gezogen wird.
/// Alle uebrigen Ereignisse verwirft sie vorerst.
///
/// **Den Frame zeichnet sie selbst**: leeren, die IsoMapView, darauf die
/// Markierung des Weges einer ziehenden Einheit, die die ContextMenuView
/// zeichnet (RAD_UpdateContextMenuViewPathOverlay), die sichtbaren von
/// TerrainInfoView und UnitInfoView, die EndTurnView, die UnitDeploymentView,
/// die ContextMenuView, die DebugView, anzeigen.
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
/// "session" (io/net_session.h): die RootView liest daraus die eigene
/// Spieler-Id, ihre ContextMenuView schickt darueber Deploy-Kommandos, ihre
/// EndTurnView das Abgeben. Und "world": die IsoMapView beobachtet sie
/// (view/iso_map_view.h), die RootView holt daraus beim Oeffnen der
/// ContextMenuView die Reserve des eigenen Spielers, die EndTurnView liest
/// daraus, wer dran ist. Und "focus": die
/// IsoMapView schreibt ihn, die TerrainInfoView liest ihn, und die RootView
/// liest ihn bei einem Linksklick, um die ContextMenuView ueber das angeklickte
/// Feld zu setzen.
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
    RAD_IoNetSession_t *session,
    const RAD_ClientWorld_t *world,
    RAD_ClientFocus_t *focus
);
void RAD_DestroyView(RAD_RootView_t **view);

///
/// Gibt alle anstehenden SDL-Ereignisse an Kamera-Steuerung, ContextMenuView
/// und IsoMapView, verwirft den Rest und zeichnet danach den Frame. Einmal je
/// Frame zu rufen.
///
void RAD_UpdateView(RAD_RootView_t *view);

///
/// Setzt (true) oder ruecksetzt (false), ob TerrainInfoView bzw. UnitInfoView
/// sichtbar sind. Die UnitInfoView braucht dazu noch ein ausgewaehltes Feld mit
/// Einheit (siehe oben).
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
