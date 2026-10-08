#ifndef __RAD_VIEW_CONTEXT_MENU_CONTEXT_MENU_STATE_MACHINE_H__
#define __RAD_VIEW_CONTEXT_MENU_CONTEXT_MENU_STATE_MACHINE_H__

#include <radish/model/tile.h>

///
/// Die Zustandsmaschine der ContextMenuView
/// (view/context_menu/context_menu_view.h).
///
/// Ihre Zustaende stehen in RAD_ContextMenuState_t; Uebergaenge loesen die
/// Funktionen RAD_ContextMenuStateMachineOn... aus. Der Weg zum Deployen:
///
///   IDLE --OnTileSelected (freies Feld)--> EMPTY_TILE_SELECTED
///        --OnDeploymentRequested--> SELECT_ENTITY_FROM_RESERVE
///        --OnEntityFromReserveSelected--> WAITING_FOR_USER_ACKNOWLEDGEMENT
///        --OnDeploymentRequested--> IDLE
///
/// Der Weg zum Ziehen; den Weg, den die Einheit nehmen soll, klickt der
/// Spieler in ENTITY_MOVE Feld fuer Feld zusammen (die ContextMenuView haelt
/// ihn):
///
///   IDLE --OnTileSelected (eigene Einheit)--> TILE_WITH_OWN_ENTITY_SELECTED
///        --OnMoveRequested--> ENTITY_MOVE
///        --OnPathCompleted--> WAITING_FOR_MOVE_ACKNOWLEDGEMENT
///        --OnMoveRequested--> IDLE
///
/// Abbrechen laesst sich der Weg mit einem Klick auf das Feld der Einheit:
///
///   ENTITY_MOVE --OnMoveAborted--> TILE_WITH_OWN_ENTITY_SELECTED
///
/// Der Weg zum Angreifen; das Zielfeld klickt der Spieler in ENTITY_ATTACK an
/// (die ContextMenuView prueft es und haelt es):
///
///   TILE_WITH_OWN_ENTITY_SELECTED --OnAttackRequested--> ENTITY_ATTACK
///        --OnTargetSelected--> WAITING_FOR_ATTACK_ACKNOWLEDGEMENT
///        --OnAttackRequested--> IDLE
///
/// Abbrechen laesst sich die Zielwahl mit einem Klick auf das Feld der
/// Einheit:
///
///   ENTITY_ATTACK --OnAttackAborted--> TILE_WITH_OWN_ENTITY_SELECTED
///
/// **Jeder Zustandswechsel geht ins Log**, mit dem Ereignis, das ihn
/// ausgeloest hat, etwa "[ContextMenuStateMachine] OnCancel:
/// EMPTY_TILE_SELECTED -> IDLE". Bleibt der Zustand derselbe, geht nichts ins
/// Log.
///

///
/// Die Zustaende der Zustandsmaschine.
///
typedef enum
{
    /// Nichts ausgewaehlt, die ContextMenuView ist geschlossen.
    RAD_CONTEXT_MENU_STATE_IDLE = 0,
    /// Ein Feld ohne Einheit ist ausgewaehlt.
    RAD_CONTEXT_MENU_STATE_EMPTY_TILE_SELECTED,
    /// Ein Feld mit einer Einheit des Spielers ist ausgewaehlt.
    RAD_CONTEXT_MENU_STATE_TILE_WITH_OWN_ENTITY_SELECTED,
    /// Ein Feld mit einer Einheit ist ausgewaehlt, die nicht dem Spieler
    /// gehoert -- einem anderen oder niemandem.
    RAD_CONTEXT_MENU_STATE_TILE_WITH_FOREIGN_ENTITY_SELECTED,
    /// Eine Einheit wird aus der Reserve gewaehlt (UnitDeploymentView).
    RAD_CONTEXT_MENU_STATE_SELECT_ENTITY_FROM_RESERVE,
    /// Eine Einheit aus der Reserve ist gewaehlt; der Spieler muss das
    /// Deployen noch einmal bestaetigen (OnDeploymentRequested).
    RAD_CONTEXT_MENU_STATE_WAITING_FOR_USER_ACKNOWLEDGEMENT,
    /// Eine Einheit aus der Reserve wird deployt.
    RAD_CONTEXT_MENU_STATE_DEPLOY_FROM_RESERVE,
    /// Die Einheit auf dem ausgewaehlten Feld soll ziehen; der Spieler klickt
    /// ihren Weg zusammen.
    RAD_CONTEXT_MENU_STATE_ENTITY_MOVE,
    /// Der Weg steht; der Spieler muss das Ziehen noch bestaetigen
    /// (OnMoveRequested).
    RAD_CONTEXT_MENU_STATE_WAITING_FOR_MOVE_ACKNOWLEDGEMENT,
    /// Die Einheit auf dem ausgewaehlten Feld soll angreifen; der Spieler
    /// klickt ihr Zielfeld an.
    RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK,
    /// Das Zielfeld steht; der Spieler muss den Angriff noch bestaetigen
    /// (OnAttackRequested).
    RAD_CONTEXT_MENU_STATE_WAITING_FOR_ATTACK_ACKNOWLEDGEMENT,
    /// Eine Anfrage ist an den Server geschickt, die Antwort steht aus.
    RAD_CONTEXT_MENU_STATE_WAITING_FOR_SERVER_RESPONSE
} RAD_ContextMenuState_t;

///
/// Die Zustandsmaschine selbst: nur ein Name. Was sie haelt, steht in
/// view/context_menu/context_menu_state_machine.c.
///
typedef struct RAD_ContextMenuStateMachine RAD_ContextMenuStateMachine_t;

///
/// Legt eine Zustandsmaschine im Zustand RAD_CONTEXT_MENU_STATE_IDLE an; NULL,
/// wenn kein Speicher da ist. RAD_DestroyContextMenuStateMachine nullt den
/// Zeiger des Aufrufers.
///
RAD_ContextMenuStateMachine_t* RAD_CreateContextMenuStateMachine(void);
void RAD_DestroyContextMenuStateMachine(RAD_ContextMenuStateMachine_t **state_machine);

///
/// Der Zustand, in dem sie gerade ist.
///
RAD_ContextMenuState_t RAD_ContextMenuStateMachineState(const RAD_ContextMenuStateMachine_t *state_machine);

///
/// Wertet aus, dass der Spieler "player" das Feld "tile" ausgewaehlt hat --
/// aus der Welt (model/world.h), NULL fuer keins, etwa neben die Felder
/// geklickt. Je nach Zustand fuehrt das in einen anderen:
///
///   - IDLE: ein Feld mit einer Einheit von "player" nach
///     TILE_WITH_OWN_ENTITY_SELECTED, mit einer anderen Einheit nach
///     TILE_WITH_FOREIGN_ENTITY_SELECTED, ein freies nach
///     EMPTY_TILE_SELECTED; keins: bleibt bei IDLE. Ist "player"
///     RAD_CLIENT_PLAYER_NONE -- der Spieler ist noch unbekannt --, gehoert
///     ihm keine Einheit, auch keine herrenlose.
///   - in jedem anderen Zustand: bleibt, gleich, welches Feld. In ENTITY_MOVE
///     verlaengert ein Feld den Weg, in ENTITY_ATTACK ist es das Ziel -- das
///     macht die ContextMenuView, nicht die Zustandsmaschine.
///
void RAD_ContextMenuStateMachineOnTileSelected(RAD_ContextMenuStateMachine_t *state_machine, const RAD_ClientTile_t *tile, RAD_ClientPlayerId_t player);

///
/// Bricht ab: aus jedem Zustand zurueck nach IDLE.
///
void RAD_ContextMenuStateMachineOnCancel(RAD_ContextMenuStateMachine_t *state_machine);

///
/// Wertet aus, dass das Deployen einer Einheit aus der Reserve verlangt wurde:
///
///   - EMPTY_TILE_SELECTED: nach SELECT_ENTITY_FROM_RESERVE.
///   - WAITING_FOR_USER_ACKNOWLEDGEMENT: der Spieler bestaetigt, nach IDLE.
///   - in jedem anderen Zustand: nach IDLE.
///
void RAD_ContextMenuStateMachineOnDeploymentRequested(RAD_ContextMenuStateMachine_t *state_machine);

///
/// Wertet aus, dass die Einheit auf dem ausgewaehlten Feld ziehen soll:
///
///   - TILE_WITH_OWN_ENTITY_SELECTED: nach ENTITY_MOVE.
///   - ENTITY_MOVE: bleibt.
///   - WAITING_FOR_MOVE_ACKNOWLEDGEMENT: der Spieler bestaetigt, nach IDLE.
///   - in jedem anderen Zustand: nach IDLE.
///
void RAD_ContextMenuStateMachineOnMoveRequested(RAD_ContextMenuStateMachine_t *state_machine);

///
/// Wertet aus, dass der Weg der ziehenden Einheit steht: aus ENTITY_MOVE nach
/// WAITING_FOR_MOVE_ACKNOWLEDGEMENT, aus jedem anderen Zustand nach IDLE.
///
void RAD_ContextMenuStateMachineOnPathCompleted(RAD_ContextMenuStateMachine_t *state_machine);

///
/// Wertet aus, dass der Spieler den Weg abbricht: aus ENTITY_MOVE zurueck nach
/// TILE_WITH_OWN_ENTITY_SELECTED -- die Einheit bleibt ausgewaehlt --, aus
/// jedem anderen Zustand nach IDLE.
///
void RAD_ContextMenuStateMachineOnMoveAborted(RAD_ContextMenuStateMachine_t *state_machine);

///
/// Wertet aus, dass die Einheit auf dem ausgewaehlten Feld angreifen soll:
///
///   - TILE_WITH_OWN_ENTITY_SELECTED: nach ENTITY_ATTACK.
///   - ENTITY_ATTACK: bleibt.
///   - WAITING_FOR_ATTACK_ACKNOWLEDGEMENT: der Spieler bestaetigt, nach IDLE.
///   - in jedem anderen Zustand: nach IDLE.
///
void RAD_ContextMenuStateMachineOnAttackRequested(RAD_ContextMenuStateMachine_t *state_machine);

///
/// Wertet aus, dass das Ziel des Angriffs steht: aus ENTITY_ATTACK nach
/// WAITING_FOR_ATTACK_ACKNOWLEDGEMENT, aus jedem anderen Zustand nach IDLE.
///
void RAD_ContextMenuStateMachineOnTargetSelected(RAD_ContextMenuStateMachine_t *state_machine);

///
/// Wertet aus, dass der Spieler die Zielwahl abbricht: aus ENTITY_ATTACK
/// zurueck nach TILE_WITH_OWN_ENTITY_SELECTED -- die Einheit bleibt
/// ausgewaehlt --, aus jedem anderen Zustand nach IDLE.
///
void RAD_ContextMenuStateMachineOnAttackAborted(RAD_ContextMenuStateMachine_t *state_machine);

///
/// Wertet aus, dass eine Einheit aus der Reserve ausgewaehlt wurde: aus
/// SELECT_ENTITY_FROM_RESERVE nach WAITING_FOR_USER_ACKNOWLEDGEMENT, aus
/// jedem anderen Zustand nach IDLE.
///
void RAD_ContextMenuStateMachineOnEntityFromReserveSelected(RAD_ContextMenuStateMachine_t *state_machine);

///
/// Wertet aus, dass eine Bestaetigung eingetroffen ist: aus jedem Zustand
/// nach IDLE.
///
void RAD_ContextMenuStateMachineOnAcknowledgementReceived(RAD_ContextMenuStateMachine_t *state_machine);

#endif
