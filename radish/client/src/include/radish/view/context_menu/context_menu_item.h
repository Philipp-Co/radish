#ifndef __RAD_VIEW_CONTEXT_MENU_CONTEXT_MENU_ITEM_H__
#define __RAD_VIEW_CONTEXT_MENU_CONTEXT_MENU_ITEM_H__

#include <SDL2/SDL.h>
#include <stdbool.h>
#include <stdint.h>

///
/// Ein Eintrag der ContextMenuView (view/context_menu/context_menu_view.h): ein gefuellter
/// Kreis mit Mittelpunkt, Radius und Farbe. Was er bedeutet, weiss nur der
/// Aufrufer.
///
/// **Wo er steht, bestimmt die ContextMenuView**: sie liest seinen Winkel
/// (RAD_ContextMenuItemAngle) und setzt daraus seinen Mittelpunkt
/// (RAD_ContextMenuItemSetCenter). Er selbst kennt den Ring nicht.
///
/// **Maus-Ereignisse bekommt er von der ContextMenuView**, in
/// Fensterkoordinaten, wie SDL sie liefert. Er merkt sich, ob die Maus ueber
/// ihm ist, und schreibt Wechsel und Klicks ins Log.
///
/// **Was ein Klick bewirkt, sagt seine Aktion** (RAD_ContextMenuItemAction_t,
/// RAD_ContextMenuItemSetAction); ausfuehren tut sie die ContextMenuView.
/// Anfangs hat er keine.
///
/// **Den Renderer haelt er nicht selbst**, er bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Der Eintrag selbst: nur ein Name. Was er haelt, steht in
/// view/context_menu/context_menu_item.c.
///
typedef struct RAD_ContextMenuItem RAD_ContextMenuItem_t;

///
/// Was die ContextMenuView tut, wenn der Eintrag angeklickt wird -- die linke
/// Maustaste ueber ihm losgelassen (view/context_menu/context_menu_view.h).
///
typedef enum
{
    /// Nichts.
    RAD_CONTEXT_MENU_ITEM_ACTION_NONE = 0,
    /// Ihre Zustandsmaschine bricht ab (RAD_ContextMenuStateMachineOnCancel).
    RAD_CONTEXT_MENU_ITEM_ACTION_CLOSE,
    /// Ihre Zustandsmaschine soll deployen
    /// (RAD_ContextMenuStateMachineOnDeploymentRequested).
    RAD_CONTEXT_MENU_ITEM_ACTION_DEPLOY,
    /// Die Einheit auf dem ausgewaehlten Feld soll ziehen. Vorerst nur ein
    /// Eintrag im Log.
    RAD_CONTEXT_MENU_ITEM_ACTION_MOVE,
    /// Die Einheit auf dem ausgewaehlten Feld soll angreifen. Vorerst nur ein
    /// Eintrag im Log.
    RAD_CONTEXT_MENU_ITEM_ACTION_ATTACK
} RAD_ContextMenuItemAction_t;

///
/// Legt einen Eintrag am Winkel "angle" in Grad (view/context_menu/context_menu_view.h)
/// mit dem Radius "radius" in Pixeln und der Farbe "color" an, mit dem
/// Mittelpunkt bei (0, 0), die Maus nicht ueber ihm, ohne Aktion; NULL, wenn
/// kein Speicher da ist. RAD_DestroyContextMenuItem nullt den Zeiger des
/// Aufrufers.
///
RAD_ContextMenuItem_t* RAD_CreateContextMenuItem(float angle, int32_t radius, SDL_Color color);
void RAD_DestroyContextMenuItem(RAD_ContextMenuItem_t **item);

///
/// Sein Winkel auf dem Ring, in Grad, wie bei RAD_CreateContextMenuItem.
///
float RAD_ContextMenuItemAngle(const RAD_ContextMenuItem_t *item);

///
/// Setzt seinen Mittelpunkt, in Fensterkoordinaten.
///
void RAD_ContextMenuItemSetCenter(RAD_ContextMenuItem_t *item, float center_x, float center_y);

///
/// Seine Aktion bei einem Klick, setzen und abfragen.
///
void RAD_ContextMenuItemSetAction(RAD_ContextMenuItem_t *item, RAD_ContextMenuItemAction_t action);
RAD_ContextMenuItemAction_t RAD_ContextMenuItemAction(const RAD_ContextMenuItem_t *item);

///
/// Farbe des Eintrags.
///
void RAD_ContextMenuItemSetColor(RAD_ContextMenuItem_t *item, SDL_Color color);

///
/// Ob der Punkt (x, y) in seinem Kreis liegt, Rand eingeschlossen.
///
bool RAD_ContextMenuItemContains(const RAD_ContextMenuItem_t *item, int32_t x, int32_t y);

///
/// Setzt (true) oder ruecksetzt (false), ob er hervorgehoben ist: dann wird
/// er blau gezeichnet statt in seiner Farbe, die dabei erhalten bleibt.
/// Anfangs ist er es nicht.
///
void RAD_ContextMenuItemSetHighlighted(RAD_ContextMenuItem_t *item, bool highlighted);

///
/// Ob die Maus gerade ueber ihm ist (RAD_ContextMenuItemHandleMouseMotion).
///
bool RAD_ContextMenuItemIsHovered(const RAD_ContextMenuItem_t *item);

///
/// Wertet eine Mausbewegung aus: liegt der Zeiger in seinem Kreis, ist die
/// Maus ueber ihm, sonst nicht. Ein Wechsel geht ins Log.
///
void RAD_ContextMenuItemHandleMouseMotion(RAD_ContextMenuItem_t *item, const SDL_MouseMotionEvent *motion);

///
/// Wertet das Druecken oder Loslassen einer Maustaste aus. true, wenn es in
/// seinem Kreis liegt -- dann gehoert es ihm; geht ins Log. false sonst.
///
bool RAD_ContextMenuItemHandleMouseButton(RAD_ContextMenuItem_t *item, const SDL_MouseButtonEvent *button);

///
/// Zeichnet den Eintrag mit "renderer". Einmal je Frame zu rufen, von der
/// ContextMenuView.
///
void RAD_UpdateContextMenuItem(RAD_ContextMenuItem_t *item, SDL_Renderer *renderer);

#endif
