#ifndef __RAD_VIEW_CONTEXT_MENU_CONTEXT_MENU_VIEW_H__
#define __RAD_VIEW_CONTEXT_MENU_CONTEXT_MENU_VIEW_H__

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdbool.h>
#include <stdint.h>
#include <radish/io/net_session.h>
#include <radish/model/reserve.h>
#include <radish/model/world.h>
#include <radish/rendering/iso_map.h>
#include <radish/view/context_menu/context_menu_item.h>
#include <radish/view/context_menu/context_menu_state_machine.h>
#include <radish/view/unit_deployment_view.h>

///
/// Ein Kreismenue: ein Ring um einen Mittelpunkt, auf dem kleine runde
/// Eintraege sitzen, die ContextMenuItems (view/context_menu/context_menu_item.h). Was ein
/// Eintrag bedeutet, weiss nur der Aufrufer.
///
/// **Der Ring liegt mittig auf dem Radius**: er reicht die halbe Ringbreite
/// nach innen und nach aussen. Innen ist er leer, was darunter liegt, bleibt
/// zu sehen.
///
/// **Jeder Eintrag steht an seinem eigenen Winkel auf dem Ring**, in Grad:
/// 0 ist oben, es geht im Uhrzeigersinn -- 45 ist oben rechts, 225 unten
/// links. Der Mittelpunkt eines Eintrags liegt genau auf dem Radius, also
/// mitten im Ring; die ContextMenuView setzt ihn bei jedem Verschieben neu.
/// Ueberlappen sich zwei, liegt der spaeter hinzugefuegte oben.
///
/// **Welche Eintraege sie hat, folgt aus dem Zustand ihrer Zustandsmaschine**
/// (siehe unten); sie baut sie bei jedem Zustandswechsel selbst neu, von
/// aussen werden keine hinzugefuegt. Ausser in IDLE, ENTITY_MOVE und
/// ENTITY_ATTACK -- da ist sie unsichtbar und hat keine -- steht immer unten
/// links
/// Schliessen (RAD_CONTEXT_MENU_ITEM_ACTION_CLOSE), rechts davon, oben auf dem
/// Scheitel beginnend:
///
///   - EMPTY_TILE_SELECTED, SELECT_ENTITY_FROM_RESERVE, DEPLOY_FROM_RESERVE:
///     Deploy (RAD_CONTEXT_MENU_ITEM_ACTION_DEPLOY).
///   - WAITING_FOR_USER_ACKNOWLEDGEMENT: Deploy, blau hervorgehoben
///     (RAD_ContextMenuItemSetHighlighted).
///   - TILE_WITH_OWN_ENTITY_SELECTED: Move (RAD_CONTEXT_MENU_ITEM_ACTION_MOVE)
///     und Attack (RAD_CONTEXT_MENU_ITEM_ACTION_ATTACK) -- jeder nur, solange
///     die Einheit ihn in diesem Zug noch darf: nicht in diesem Zug aufgestellt
///     und nicht schon gezogen bzw. angegriffen (RAD_ClientUnit_t.deployed,
///     moved, attacked, gelesen beim Oeffnen). Darf sie keins von beiden, steht
///     nur Schliessen da.
///   - WAITING_FOR_MOVE_ACKNOWLEDGEMENT: Move, blau hervorgehoben.
///   - WAITING_FOR_ATTACK_ACKNOWLEDGEMENT: Attack, blau hervorgehoben.
///   - TILE_WITH_FOREIGN_ENTITY_SELECTED -- die Einheit gehoert nicht dem
///     Spieler dieses Clients (own_player_id, io/net_session.h) --,
///     WAITING_FOR_SERVER_RESPONSE: nur Schliessen.
///
/// Die Eintraege gehoeren ihr; Zeiger auf sie gibt sie nicht heraus.
///
/// **Maus-Ereignisse bekommt sie vom Aufrufer** (view/view.h), in
/// Fensterkoordinaten, wie SDL sie liefert, **und gibt sie an ihre Eintraege
/// weiter**, die selbst entscheiden, ob sie sie treffen
/// (RAD_ContextMenuItemHandleMouseMotion, RAD_ContextMenuItemHandleMouseButton).
/// Wird einer angeklickt -- die linke Maustaste ueber ihm losgelassen --,
/// meldet sie seine Aktion (RAD_ContextMenuItemAction_t) ihrer
/// Zustandsmaschine: RAD_CONTEXT_MENU_ITEM_ACTION_CLOSE als OnCancel,
/// RAD_CONTEXT_MENU_ITEM_ACTION_DEPLOY als OnDeploymentRequested,
/// RAD_CONTEXT_MENU_ITEM_ACTION_MOVE als OnMoveRequested,
/// RAD_CONTEXT_MENU_ITEM_ACTION_ATTACK als OnAttackRequested; diese beiden
/// gehen dazu mit dem ausgewaehlten Feld ins Log.
///
/// **Sie hat eine UnitDeploymentView** (view/unit_deployment_view.h), die ihr
/// gehoert. Die oeffnet sich mit ihrer oberen linken Ecke dort, wo Deploy
/// angeklickt wurde, und zeigt die Einheiten der Reserve aus
/// RAD_ContextMenuViewSetReserve -- eine Referenz, beobachtet wird die
/// Reserve nicht. Sie liegt ueber dem Ring und bekommt Maus-Ereignisse vor den
/// Eintraegen. Wird darin eine Einheit ausgewaehlt, geht sie ins Log, und die
/// Zustandsmaschine erfaehrt es als OnEntityFromReserveSelected. Sie bleibt
/// stehen, wo sie geoeffnet wurde, auch wenn der Ring sich verschiebt.
///
/// **Ist das Spiel vorbei** (RAD_ClientWorldIsFinished), oeffnet sie sich
/// nicht mehr, und ein Klick in ein noch offenes Menue schliesst es nur.
///
/// **Bestaetigt der Spieler die gewaehlte Einheit** -- ein Klick auf Deploy in
/// WAITING_FOR_USER_ACKNOWLEDGEMENT --, schickt sie das Kommando, diese
/// Einheit auf das ausgewaehlte Feld zu stellen (RAD_ControlDeployUnit,
/// control/deploy_unit.h), ueber die Session aus RAD_CreateContextMenuView --
/// **nur, wenn der Server das Feld annehmen wuerde**: es liegt in der eigenen
/// Aufstellzone, wie der Server sie geschickt hat (RAD_ClientWorldDeployZone),
/// hat Gelaende, ist begehbar (RAD_ControlTerrainImpassable) und frei -- dieselben
/// Gruende wie im Spiel (RAD_GameCheckDeployUnit). Sonst geht der Grund ins Log,
/// kein Kommando hinaus, und Deploy ist vorher auch nicht blau hervorgehoben.
/// Angeboten wird Deploy trotzdem auf jedem freien Feld: so sieht der Spieler
/// die Zone auch, wenn er daneben geklickt hat.
///
/// **Ein Klick auf Move laesst den Spieler den Weg der Einheit
/// zusammenklicken** (ENTITY_MOVE): sie verschwindet, und jedes Feld, das der
/// Aufrufer meldet (RAD_ContextMenuViewOnTileSelected), ist ein Schritt. In
/// den Weg kommt nur ein Feld, das waagerecht oder senkrecht an das letzte
/// grenzt, frei ist und noch nicht darin steht, und hoechstens
/// RAD_NET_PATH_MAX_STEPS Felder samt dem der Einheit. **Die Reichweite der
/// Einheit ist das Budget des Weges** (RAD_ClientUnit_t.movement, gelesen beim
/// Oeffnen): jedes Feld darauf kostet RAD_ControlTerrainMovementCost
/// (control/terrain.h), das der Einheit nichts, und ein Feld, mit dem die
/// Summe die Reichweite uebersteigen wuerde, kommt nicht hinein. Alles
/// andere geht nur ins Log, mit Kosten und Verbrauch. Ein Klick auf das Feld
/// der Einheit bricht den Weg ab: zurueck nach TILE_WITH_OWN_ENTITY_SELECTED,
/// sie oeffnet sich dort wieder mit Move und Attack. Hat der Weg wenigstens
/// einen Schritt, schliesst ein zweiter Klick auf sein letztes Feld ihn ab
/// (WAITING_FOR_MOVE_ACKNOWLEDGEMENT): sie oeffnet sich ueber diesem Feld
/// (RAD_ContextMenuViewAnchorTile). Ein Klick auf Move schickt dann das
/// Kommando, die Einheit diesen Weg ziehen zu lassen (RAD_ControlMoveUnit,
/// control/move_unit.h), ein Klick auf Schliessen verwirft ihn.
///
/// **Ein Klick auf Attack laesst den Spieler das Ziel waehlen**
/// (ENTITY_ATTACK): sie verschwindet, und das naechste Feld, das der Aufrufer
/// meldet, ist das Ziel. Das Feld der Einheit selbst bricht die Zielwahl ab:
/// zurueck nach TILE_WITH_OWN_ENTITY_SELECTED, sie oeffnet sich dort wieder
/// mit Move und Attack. Jedes andere Feld ist ein Ziel, wenn keine eigene
/// Einheit darauf steht und wenigstens eine Waffe eines ihrer Mitglieder die
/// Entfernung abdeckt (RAD_ClientWeapon_t, min_range bis max_range, gelesen
/// beim Oeffnen; die Entfernung in Feldern waagerecht plus senkrecht). Auch
/// ein leeres Feld ist ein Ziel. Jedes andere geht nur ins Log, und sie wartet
/// weiter. Steht das Ziel (WAITING_FOR_ATTACK_ACKNOWLEDGEMENT), oeffnet sie
/// sich darueber; ein Klick auf Attack bestaetigt es und schickt das Kommando,
/// die Einheit dieses Feld angreifen zu lassen (RAD_ControlAttackUnit,
/// control/attack_unit.h), ein Klick auf Schliessen verwirft es.
///
/// **Den Weg markiert sie auf der Karte**, solange er zusammengeklickt wird
/// und auf die Bestaetigung wartet (ENTITY_MOVE,
/// WAITING_FOR_MOVE_ACKNOWLEDGEMENT): jedes Feld darauf als durchscheinend
/// gelbe Raute, das Ziel -- das letzte Feld, dessen zweiter Klick den Weg
/// abschliesst -- kraeftiger; das Feld der Einheit nicht. Solange er
/// zusammengeklickt wird, liegen darunter blau die Felder, die die Einheit vom
/// Ende des Weges aus noch erreicht -- nach denselben Regeln wie ein Klick,
/// mit dem Budget, das ihr bleibt; die Welt aus RAD_CreateContextMenuView sagt,
/// welche Felder bekannt und frei sind und was sie kosten. Wo die Felder auf
/// dem Bildschirm liegen, fragt sie die Karte aus RAD_CreateContextMenuView
/// (RAD_IsoMapTileScreenCenter); was dort nicht gezeichnet wird, markiert sie
/// nicht.
///
/// **Beim Angriff markiert sie die Felder in Reichweite**, solange das Ziel
/// gesucht wird und auf die Bestaetigung wartet (ENTITY_ATTACK,
/// WAITING_FOR_ATTACK_ACKNOWLEDGEMENT): jedes Feld, das wenigstens eine Waffe
/// der Einheit erreicht -- dieselbe Regel wie bei der Zielwahl --, als
/// durchscheinend rote Raute, das gewaehlte Ziel kraeftiger; das Feld der
/// Einheit nicht.
///
/// **Beim Deployen markiert sie die eigene Aufstellzone**, ab dem Klick auf
/// Deploy bis zur Bestaetigung (SELECT_ENTITY_FROM_RESERVE,
/// DEPLOY_FROM_RESERVE, WAITING_FOR_USER_ACKNOWLEDGEMENT): jedes Feld der Zone,
/// auf das sich aufstellen laesst, durchscheinend gruen -- unbegehbare, besetzte
/// und solche ohne Gelaende bleiben frei --, das ausgewaehlte Feld kraeftiger:
/// gruen, wenn Deploy es annimmt, rot, wenn nicht. Die Zone kommt aus der Welt, fuer den
/// Spieler dieses Clients (own_player_id der Session).
///
/// Gezeichnet werden alle Markierungen als Overlay fuer sich
/// (RAD_UpdateContextMenuViewOverlay), damit der Aufrufer es unter seine
/// anderen Views legen kann.
///
/// **Was sie zeichnet, folgt aus ihrer Zustandsmaschine**
/// (view/context_menu/context_menu_state_machine.h), die ihr gehoert und im
/// Zustand IDLE beginnt:
///
///   - IDLE, ENTITY_MOVE, ENTITY_ATTACK: nichts; sie nimmt dann auch keine
///     Maus-Ereignisse an.
///   - SELECT_ENTITY_FROM_RESERVE: Ring, Eintraege und UnitDeploymentView.
///   - jeder andere Zustand: Ring und die Eintraege des Zustands (siehe oben).
///
/// Ihr Mittelpunkt und alles andere bleiben dabei, wie sie sind. Eine
/// Feldauswahl meldet der Aufrufer (RAD_ContextMenuViewOnTileSelected).
///
/// **Den Renderer haelt sie nicht selbst**, sie bekommt ihn bei jedem Update
/// (view/view.h).
///

///
/// Die Komponente selbst: nur ein Name. Was sie haelt, steht in
/// view/context_menu/context_menu_view.c.
///
typedef struct RAD_ContextMenuView RAD_ContextMenuView_t;

///
/// Legt eine ContextMenuView mit dem Mittelpunkt (center_x, center_y) und dem
/// Radius "radius" in Pixeln an, in IDLE -- also unsichtbar --, ohne
/// Eintraege, mit einem grauen, 8 Pixel breiten Ring, ohne Reserve. Ihre UnitDeploymentView ist
/// deployment_width x deployment_height Pixel gross und schreibt in "font";
/// die Schrift gehoert dem Aufrufer (view/view.h, RAD_ViewFont) und muss die
/// ContextMenuView ueberleben, ebenso "session", ueber die Deploy-,
/// Move- und Attack-Kommandos hinausgehen, "map", auf der sie Weg und
/// Reichweite markiert, und "world", aus der sie liest, wohin eine Einheit
/// ziehen kann -- beide nur gelesen. NULL, wenn kein Speicher da ist.
/// RAD_DestroyContextMenuView baut auch alle Eintraege, die UnitDeploymentView
/// und die Zustandsmaschine ab und nullt den Zeiger des Aufrufers.
///
RAD_ContextMenuView_t* RAD_CreateContextMenuView(int32_t center_x, int32_t center_y, int32_t radius,
                                                 TTF_Font *font, int32_t deployment_width, int32_t deployment_height,
                                                 RAD_IoNetSession_t *session, const RAD_IsoMap_t *map,
                                                 const RAD_ClientWorld_t *world);
void RAD_DestroyContextMenuView(RAD_ContextMenuView_t **context_menu_view);

///
/// Verschiebt den Mittelpunkt; die Eintraege wandern mit.
///
void RAD_ContextMenuViewSetCenter(RAD_ContextMenuView_t *context_menu_view, int32_t center_x, int32_t center_y);

///
/// Meldet ihrer Zustandsmaschine, dass das Feld "tile" ausgewaehlt wurde --
/// aus der Welt, NULL fuer keins (RAD_ContextMenuStateMachineOnTileSelected).
/// Ausgewaehlt hat es der Spieler dieses Clients, own_player_id aus der
/// Session von RAD_CreateContextMenuView. In ENTITY_MOVE ist es statt dessen
/// ein Schritt auf dem Weg der Einheit, in ENTITY_ATTACK ihr Ziel (siehe
/// oben).
///
void RAD_ContextMenuViewOnTileSelected(RAD_ContextMenuView_t *context_menu_view, const RAD_ClientTile_t *tile);

///
/// Ob ein Feld ausgewaehlt ist: ihre Zustandsmaschine ist nicht in IDLE --
/// auch, waehrend der Weg einer Einheit zusammengeklickt oder ihr Ziel gesucht
/// wird und sie nicht zu sehen ist.
///
bool RAD_ContextMenuViewIsActive(const RAD_ContextMenuView_t *context_menu_view);

///
/// Ob sie zu sehen ist: ihre Zustandsmaschine ist weder in IDLE noch in
/// ENTITY_MOVE oder ENTITY_ATTACK.
///
bool RAD_ContextMenuViewIsVisible(const RAD_ContextMenuView_t *context_menu_view);

///
/// Das ausgewaehlte Feld, in Weltkoordinaten: das, mit dem sie sich geoeffnet
/// hat -- auf dem die Einheit steht, die zieht oder angreift. false, wenn keins
/// ausgewaehlt ist (RAD_ContextMenuViewIsActive); dann bleiben "x" und "y",
/// wie sie sind.
///
bool RAD_ContextMenuViewSelectedTile(const RAD_ContextMenuView_t *context_menu_view, int32_t *x, int32_t *y);

///
/// Das Feld, ueber dem sie steht, in Weltkoordinaten: das ausgewaehlte, in
/// WAITING_FOR_MOVE_ACKNOWLEDGEMENT das Ziel des Weges, in
/// WAITING_FOR_ATTACK_ACKNOWLEDGEMENT das Ziel des Angriffs. Der Aufrufer setzt
/// ihren Mittelpunkt darauf (RAD_ContextMenuViewSetCenter). false, wenn sie
/// nicht zu sehen ist (RAD_ContextMenuViewIsVisible); dann bleiben "x" und
/// "y", wie sie sind.
///
bool RAD_ContextMenuViewAnchorTile(const RAD_ContextMenuView_t *context_menu_view, int32_t *x, int32_t *y);

///
/// Die Reserve, deren Einheiten die UnitDeploymentView zeigt, und das
/// Repository, in dem sie stehen; NULL fuer keine. Nur Referenzen
/// (RAD_UnitDeploymentViewSetReserve): sie gehoeren der Welt und werden beim
/// naechsten Oeffnen der UnitDeploymentView gelesen.
///
void RAD_ContextMenuViewSetReserve(RAD_ContextMenuView_t *context_menu_view,
                                   const RAD_ClientReserve_t *reserve,
                                   const RAD_ClientUnitRepository_t *units);

///
/// Wie breit der Ring ist, in Pixeln; negativ zaehlt als 0. Ist er breiter als
/// der doppelte Radius, wird er innen zur Scheibe.
///
void RAD_ContextMenuViewSetRingWidth(RAD_ContextMenuView_t *context_menu_view, int32_t ring_width);

///
/// Farbe des Rings. Alpha wird beachtet: was darunter liegt, scheint durch.
///
void RAD_ContextMenuViewSetColor(RAD_ContextMenuView_t *context_menu_view, SDL_Color color);

///
/// Wie viele Eintraege sie gerade hat (siehe oben).
///
int32_t RAD_ContextMenuViewItemCount(const RAD_ContextMenuView_t *context_menu_view);


///
/// Gibt eine Mausbewegung an die UnitDeploymentView weiter und, wenn die sie
/// nicht nimmt, an alle Eintraege (RAD_ContextMenuItemHandleMouseMotion).
/// true, wenn die UnitDeploymentView sie genommen hat -- dann sollte der
/// Aufrufer sie nicht an das weitergeben, was unter ihr liegt. Unsichtbar:
/// false.
///
bool RAD_ContextMenuViewHandleMouseMotion(RAD_ContextMenuView_t *context_menu_view, const SDL_MouseMotionEvent *motion);

///
/// Gibt das Druecken oder Loslassen einer Maustaste an die UnitDeploymentView
/// weiter und, wenn die es nicht nimmt, an die Eintraege, den obersten
/// zuerst, bis einer es annimmt (RAD_ContextMenuItemHandleMouseButton). true,
/// wenn eine von ihnen es angenommen hat
/// -- dann sollte der Aufrufer es nicht an das weitergeben, was unter ihr
/// liegt. false, wenn keiner, auch wenn es den Ring trifft: der ist kein
/// Eintrag. Unsichtbar: immer false, die Eintraege sehen es nicht. Ist es ein
/// Klick auf einen Eintrag, fuehrt sie dessen Aktion aus (siehe oben).
///
bool RAD_ContextMenuViewHandleMouseButton(RAD_ContextMenuView_t *context_menu_view, const SDL_MouseButtonEvent *button);

///
/// Zeichnet den Ring, darueber die Eintraege und darueber die
/// UnitDeploymentView mit "renderer" -- unsichtbar: nichts. Einmal je Frame zu rufen.
///
void RAD_UpdateContextMenuView(RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer);

///
/// Zeichnet die Markierungen (siehe oben) mit "renderer": in ENTITY_MOVE und
/// WAITING_FOR_MOVE_ACKNOWLEDGEMENT den Weg -- nichts, solange er nur aus dem
/// Feld der Einheit besteht --, in ENTITY_ATTACK und
/// WAITING_FOR_ATTACK_ACKNOWLEDGEMENT die Felder in Reichweite, ab dem Klick
/// auf Deploy bis zur Bestaetigung die eigene Aufstellzone, sonst nichts.
/// Unabhaengig davon, ob sie selbst zu sehen ist (RAD_ContextMenuViewIsVisible).
/// Einmal je Frame zu rufen, nach der Karte und vor allem, was darueber liegen
/// soll.
///
void RAD_UpdateContextMenuViewOverlay(RAD_ContextMenuView_t *context_menu_view, SDL_Renderer *renderer);

#endif
