#ifndef __RAD_VIEW_UNIT_DEPLOYMENT_VIEW_H__
#define __RAD_VIEW_UNIT_DEPLOYMENT_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/model/reserve.h>
#include <radish/model/unit_repository.h>

///
/// Ein rechteckiger Bereich mit den Einheiten einer Reserve (model/reserve.h),
/// je ein Name pro Zeile, von oben nach unten in der Reihenfolge der Reserve
/// (RAD_ClientReserveUnitAt). Die Reserve nennt nur Ids; die Namen schlaegt sie
/// im Unit-Repository nach (model/unit_repository.h).
///
/// **Die Reserve beobachtet sie nicht**, sie haelt nur eine Referenz darauf
/// (RAD_UnitDeploymentViewSetReserve) und liest sie bei jedem Einblenden neu.
/// Was sich danach an der Reserve aendert, zeigt sie erst beim naechsten
/// Einblenden.
///
/// **Die Liste kann laenger sein als die Flaeche.** Dann zeigt ein
/// Scrollbalken am rechten Rand, welcher Teil zu sehen ist, und die Liste
/// laesst sich ziehen: linke Maustaste in der Flaeche druecken und die Maus
/// nach oben oder unten bewegen -- die Liste folgt ihr. Was ueber die Flaeche
/// hinausgeht, wird abgeschnitten.
///
/// **Ausgewaehlt wird mit einem Doppelklick**: zweimal dieselbe Zeile
/// anklicken, innerhalb von 400 ms. Ein Klick ist Druecken und Loslassen der
/// linken Maustaste, ohne die Liste dabei zu ziehen. Die ausgewaehlte Einheit
/// ist hervorgehoben; abfragen laesst sie sich mit
/// RAD_UnitDeploymentViewSelectedUnit. Was eine Auswahl bewirkt, entscheidet
/// der Aufrufer.
///
/// **Maus-Ereignisse bekommt sie vom Aufrufer** (view/context_menu/context_menu_view.h), in
/// Fensterkoordinaten, wie SDL sie liefert. Waehrend die Liste gezogen wird,
/// gehoeren ihr alle Mausbewegungen, auch ausserhalb ihrer Flaeche.
///
/// **Sie ist sichtbar oder nicht** (RAD_UnitDeploymentViewSetVisible). Ist sie
/// es nicht, zeichnet sie nichts und nimmt keine Maus-Ereignisse an. Anfangs
/// ist sie unsichtbar.
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in
/// view/unit_deployment_view.c.
///
typedef struct RAD_UnitDeploymentView RAD_UnitDeploymentView_t;

///
/// Legt eine UnitDeploymentView mit der oberen linken Ecke bei (x, y) und
/// width x height Pixeln an, unsichtbar, ohne Reserve, nichts ausgewaehlt;
/// NULL, wenn kein Speicher da ist. "font" gehoert dem Aufrufer (view/view.h,
/// RAD_ViewFont) und muss die UnitDeploymentView ueberleben.
/// RAD_DestroyUnitDeploymentView nullt den Zeiger des Aufrufers.
///
RAD_UnitDeploymentView_t* RAD_CreateUnitDeploymentView(int32_t x, int32_t y, int32_t width, int32_t height, TTF_Font *font);
void RAD_DestroyUnitDeploymentView(RAD_UnitDeploymentView_t **unit_deployment_view);

///
/// Die Reserve, deren Einheiten sie zeigt, und das Repository, in dem diese
/// stehen; NULL fuer keine, dann ist die Liste leer. Beide gehoeren dem
/// Aufrufer -- der Welt (model/world.h) -- und muessen die UnitDeploymentView
/// ueberleben oder vorher ersetzt werden. Gelesen werden sie beim naechsten
/// Einblenden.
///
void RAD_UnitDeploymentViewSetReserve(RAD_UnitDeploymentView_t *unit_deployment_view,
                                      const RAD_ClientReserve_t *reserve,
                                      const RAD_ClientUnitRepository_t *units);

///
/// Verschiebt sie: ihre obere linke Ecke kommt nach (x, y), die Groesse
/// bleibt.
///
void RAD_UnitDeploymentViewSetPosition(RAD_UnitDeploymentView_t *unit_deployment_view, int32_t x, int32_t y);

///
/// Setzt (true) oder ruecksetzt (false), ob sie sichtbar ist, bzw. fragt es ab.
/// Einblenden liest die Reserve neu: die Liste steht danach ganz oben, nichts
/// ist ausgewaehlt. Beides bricht ein Ziehen ab und vergisst einen halben
/// Doppelklick.
///
void RAD_UnitDeploymentViewSetVisible(RAD_UnitDeploymentView_t *unit_deployment_view, bool visible);
bool RAD_UnitDeploymentViewIsVisible(const RAD_UnitDeploymentView_t *unit_deployment_view);

///
/// Die ausgewaehlte Einheit, aus der Reserve; NULL, wenn keine ausgewaehlt
/// ist oder sie nicht mehr in der Reserve steht. Der Zeiger gehoert dem
/// Repository. RAD_UnitDeploymentViewClearSelection nimmt die Auswahl zurueck.
///
const RAD_ClientUnit_t* RAD_UnitDeploymentViewSelectedUnit(const RAD_UnitDeploymentView_t *unit_deployment_view);
void RAD_UnitDeploymentViewClearSelection(RAD_UnitDeploymentView_t *unit_deployment_view);

///
/// Wertet eine Mausbewegung aus: zieht die Liste, wenn die linke Maustaste in
/// ihr gedrueckt wurde und die Maus sich seitdem um mehr als ein paar Pixel
/// bewegt hat. true, wenn die Bewegung ihr gehoert -- waehrend sie gezogen
/// wird oder der Zeiger in ihrer Flaeche ist --, dann sollte der Aufrufer sie
/// nicht an das weitergeben, was unter ihr liegt. Unsichtbar: false.
///
bool RAD_UnitDeploymentViewHandleMouseMotion(RAD_UnitDeploymentView_t *unit_deployment_view, const SDL_MouseMotionEvent *motion);

///
/// Wertet das Druecken oder Loslassen einer Maustaste aus: links druecken in
/// der Flaeche beginnt Ziehen oder Klick, loslassen beendet es; zwei Klicks
/// auf dieselbe Zeile waehlen deren Einheit aus. true, wenn es ihr gehoert --
/// in ihrer Flaeche oder das Ende eines Ziehens oder Klicks, der in ihr
/// begann --, dann sollte der Aufrufer es nicht an das weitergeben, was unter
/// ihr liegt. Unsichtbar: false.
///
bool RAD_UnitDeploymentViewHandleMouseButton(RAD_UnitDeploymentView_t *unit_deployment_view, const SDL_MouseButtonEvent *button);

///
/// Zeichnet Hintergrund, die sichtbaren Zeilen und den Scrollbalken mit
/// "renderer" -- unsichtbar: nichts. Einmal je Frame zu rufen.
///
void RAD_UpdateUnitDeploymentView(RAD_UnitDeploymentView_t *unit_deployment_view, SDL_Renderer *renderer);

#endif
