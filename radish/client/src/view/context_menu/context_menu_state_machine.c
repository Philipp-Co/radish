#include <radish/view/context_menu/context_menu_state_machine.h>
#include <stdio.h>
#include <stdlib.h>


///
/// Die Struktur steht hier und nicht im Header, wie bei view/view.c.
///
struct RAD_ContextMenuStateMachine
{
    ///
    /// Der Zustand, in dem sie gerade ist. Nur ueber
    /// RAD_ContextMenuStateMachineSetState zu aendern, damit jeder Wechsel ins
    /// Log geht.
    ///
    RAD_ContextMenuState_t state;
};


static void RAD_ContextMenuStateMachineSetState(RAD_ContextMenuStateMachine_t *state_machine, RAD_ContextMenuState_t state, const char *event);
static const char* RAD_ContextMenuStateName(RAD_ContextMenuState_t state);


RAD_ContextMenuStateMachine_t* RAD_CreateContextMenuStateMachine(void)
{
    RAD_ContextMenuStateMachine_t *state_machine = malloc(sizeof(struct RAD_ContextMenuStateMachine));
    if(state_machine == NULL)
    {
        return NULL;
    }

    *state_machine = (struct RAD_ContextMenuStateMachine){
        .state = RAD_CONTEXT_MENU_STATE_IDLE
    };

    return state_machine;
}

void RAD_DestroyContextMenuStateMachine(RAD_ContextMenuStateMachine_t **state_machine)
{
    free(*state_machine);
    *state_machine = NULL;
}

RAD_ContextMenuState_t RAD_ContextMenuStateMachineState(const RAD_ContextMenuStateMachine_t *state_machine)
{
    return state_machine->state;
}

void RAD_ContextMenuStateMachineOnTileSelected(RAD_ContextMenuStateMachine_t *state_machine, const RAD_ClientTile_t *tile, RAD_ClientPlayerId_t player)
{
    switch(state_machine->state)
    {
        case RAD_CONTEXT_MENU_STATE_IDLE:
        {
            // Kein Feld: es bleibt beim Leerlauf.
            if(tile == NULL)
            {
                break;
            }

            RAD_ContextMenuState_t state = RAD_CONTEXT_MENU_STATE_EMPTY_TILE_SELECTED;
            if(RAD_ClientTileHasUnit(tile))
            {
                // Ein unbekannter Spieler besitzt nichts, sonst gehoerten ihm
                // die herrenlosen Einheiten (beide RAD_CLIENT_PLAYER_NONE).
                const bool own = player != RAD_CLIENT_PLAYER_NONE && tile->unit->owner == player;
                state = own ? RAD_CONTEXT_MENU_STATE_TILE_WITH_OWN_ENTITY_SELECTED
                            : RAD_CONTEXT_MENU_STATE_TILE_WITH_FOREIGN_ENTITY_SELECTED;
            }
            RAD_ContextMenuStateMachineSetState(state_machine, state, "OnTileSelected");
            break;
        }
        default:
            break;
    }
}

void RAD_ContextMenuStateMachineOnCancel(RAD_ContextMenuStateMachine_t *state_machine)
{
    RAD_ContextMenuStateMachineSetState(state_machine, RAD_CONTEXT_MENU_STATE_IDLE, "OnCancel");
}

void RAD_ContextMenuStateMachineOnDeploymentRequested(RAD_ContextMenuStateMachine_t *state_machine)
{
    switch(state_machine->state)
    {
        case RAD_CONTEXT_MENU_STATE_EMPTY_TILE_SELECTED:
            // Deployen geht nur auf ein freies Feld: erst die Einheit waehlen.
            RAD_ContextMenuStateMachineSetState(state_machine, RAD_CONTEXT_MENU_STATE_SELECT_ENTITY_FROM_RESERVE, "OnDeploymentRequested");
            break;
        case RAD_CONTEXT_MENU_STATE_WAITING_FOR_USER_ACKNOWLEDGEMENT:
            // Die gewaehlte Einheit ist bestaetigt, das Menue ist fertig.
            RAD_ContextMenuStateMachineSetState(state_machine, RAD_CONTEXT_MENU_STATE_IDLE, "OnDeploymentRequested");
            break;
        default:
            RAD_ContextMenuStateMachineSetState(state_machine, RAD_CONTEXT_MENU_STATE_IDLE, "OnDeploymentRequested");
            break;
    }
}

void RAD_ContextMenuStateMachineOnMoveRequested(RAD_ContextMenuStateMachine_t *state_machine)
{
    switch(state_machine->state)
    {
        case RAD_CONTEXT_MENU_STATE_TILE_WITH_OWN_ENTITY_SELECTED:
            // Ziehen kann nur eine eigene Einheit.
            RAD_ContextMenuStateMachineSetState(state_machine, RAD_CONTEXT_MENU_STATE_ENTITY_MOVE, "OnMoveRequested");
            break;
        case RAD_CONTEXT_MENU_STATE_ENTITY_MOVE:
            // Sie zieht schon.
            break;
        case RAD_CONTEXT_MENU_STATE_WAITING_FOR_MOVE_ACKNOWLEDGEMENT:
            // Der Weg ist bestaetigt, das Menue ist fertig.
            RAD_ContextMenuStateMachineSetState(state_machine, RAD_CONTEXT_MENU_STATE_IDLE, "OnMoveRequested");
            break;
        default:
            RAD_ContextMenuStateMachineSetState(state_machine, RAD_CONTEXT_MENU_STATE_IDLE, "OnMoveRequested");
            break;
    }
}

void RAD_ContextMenuStateMachineOnPathCompleted(RAD_ContextMenuStateMachine_t *state_machine)
{
    // Einen Weg gibt es nur, solange er zusammengeklickt wird.
    RAD_ContextMenuStateMachineSetState(
        state_machine,
        (state_machine->state == RAD_CONTEXT_MENU_STATE_ENTITY_MOVE)
            ? RAD_CONTEXT_MENU_STATE_WAITING_FOR_MOVE_ACKNOWLEDGEMENT
            : RAD_CONTEXT_MENU_STATE_IDLE,
        "OnPathCompleted"
    );
}

void RAD_ContextMenuStateMachineOnAttackRequested(RAD_ContextMenuStateMachine_t *state_machine)
{
    switch(state_machine->state)
    {
        case RAD_CONTEXT_MENU_STATE_TILE_WITH_OWN_ENTITY_SELECTED:
            // Angreifen kann nur eine eigene Einheit.
            RAD_ContextMenuStateMachineSetState(state_machine, RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK, "OnAttackRequested");
            break;
        case RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK:
            // Sie greift schon an.
            break;
        case RAD_CONTEXT_MENU_STATE_WAITING_FOR_ATTACK_ACKNOWLEDGEMENT:
            // Das Ziel ist bestaetigt, das Menue ist fertig.
            RAD_ContextMenuStateMachineSetState(state_machine, RAD_CONTEXT_MENU_STATE_IDLE, "OnAttackRequested");
            break;
        default:
            RAD_ContextMenuStateMachineSetState(state_machine, RAD_CONTEXT_MENU_STATE_IDLE, "OnAttackRequested");
            break;
    }
}

void RAD_ContextMenuStateMachineOnTargetSelected(RAD_ContextMenuStateMachine_t *state_machine)
{
    // Ein Ziel gibt es nur, solange danach gefragt wird.
    RAD_ContextMenuStateMachineSetState(
        state_machine,
        (state_machine->state == RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK)
            ? RAD_CONTEXT_MENU_STATE_WAITING_FOR_ATTACK_ACKNOWLEDGEMENT
            : RAD_CONTEXT_MENU_STATE_IDLE,
        "OnTargetSelected"
    );
}

void RAD_ContextMenuStateMachineOnMoveAborted(RAD_ContextMenuStateMachine_t *state_machine)
{
    // Abbrechen laesst sich nur der Weg, der gerade zusammengeklickt wird.
    RAD_ContextMenuStateMachineSetState(
        state_machine,
        (state_machine->state == RAD_CONTEXT_MENU_STATE_ENTITY_MOVE)
            ? RAD_CONTEXT_MENU_STATE_TILE_WITH_OWN_ENTITY_SELECTED
            : RAD_CONTEXT_MENU_STATE_IDLE,
        "OnMoveAborted"
    );
}

void RAD_ContextMenuStateMachineOnAttackAborted(RAD_ContextMenuStateMachine_t *state_machine)
{
    // Abbrechen laesst sich nur die laufende Zielwahl.
    RAD_ContextMenuStateMachineSetState(
        state_machine,
        (state_machine->state == RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK)
            ? RAD_CONTEXT_MENU_STATE_TILE_WITH_OWN_ENTITY_SELECTED
            : RAD_CONTEXT_MENU_STATE_IDLE,
        "OnAttackAborted"
    );
}

void RAD_ContextMenuStateMachineOnEntityFromReserveSelected(RAD_ContextMenuStateMachine_t *state_machine)
{
    // Eine Einheit aus der Reserve gibt es nur, solange danach gefragt wird.
    RAD_ContextMenuStateMachineSetState(
        state_machine,
        (state_machine->state == RAD_CONTEXT_MENU_STATE_SELECT_ENTITY_FROM_RESERVE)
            ? RAD_CONTEXT_MENU_STATE_WAITING_FOR_USER_ACKNOWLEDGEMENT
            : RAD_CONTEXT_MENU_STATE_IDLE,
        "OnEntityFromReserveSelected"
    );
}

void RAD_ContextMenuStateMachineOnAcknowledgementReceived(RAD_ContextMenuStateMachine_t *state_machine)
{
    RAD_ContextMenuStateMachineSetState(state_machine, RAD_CONTEXT_MENU_STATE_IDLE, "OnAcknowledgementReceived");
}


///
/// Wechselt nach "state" und schreibt den Wechsel ins Log, mit dem Ereignis
/// "event", das ihn ausgeloest hat. Bleibt der Zustand derselbe, geht nichts
/// ins Log.
///
static void RAD_ContextMenuStateMachineSetState(RAD_ContextMenuStateMachine_t *state_machine, RAD_ContextMenuState_t state, const char *event)
{
    if(state == state_machine->state)
    {
        return;
    }

    printf("[ContextMenuStateMachine] %s: %s -> %s\n",
           event, RAD_ContextMenuStateName(state_machine->state), RAD_ContextMenuStateName(state));
    state_machine->state = state;
}

///
/// Der Name eines Zustands fuers Log, wie im Enum ohne Praefix.
///
static const char* RAD_ContextMenuStateName(RAD_ContextMenuState_t state)
{
    switch(state)
    {
        case RAD_CONTEXT_MENU_STATE_IDLE:
            return "IDLE";
        case RAD_CONTEXT_MENU_STATE_EMPTY_TILE_SELECTED:
            return "EMPTY_TILE_SELECTED";
        case RAD_CONTEXT_MENU_STATE_TILE_WITH_OWN_ENTITY_SELECTED:
            return "TILE_WITH_OWN_ENTITY_SELECTED";
        case RAD_CONTEXT_MENU_STATE_TILE_WITH_FOREIGN_ENTITY_SELECTED:
            return "TILE_WITH_FOREIGN_ENTITY_SELECTED";
        case RAD_CONTEXT_MENU_STATE_SELECT_ENTITY_FROM_RESERVE:
            return "SELECT_ENTITY_FROM_RESERVE";
        case RAD_CONTEXT_MENU_STATE_WAITING_FOR_USER_ACKNOWLEDGEMENT:
            return "WAITING_FOR_USER_ACKNOWLEDGEMENT";
        case RAD_CONTEXT_MENU_STATE_DEPLOY_FROM_RESERVE:
            return "DEPLOY_FROM_RESERVE";
        case RAD_CONTEXT_MENU_STATE_ENTITY_MOVE:
            return "ENTITY_MOVE";
        case RAD_CONTEXT_MENU_STATE_WAITING_FOR_MOVE_ACKNOWLEDGEMENT:
            return "WAITING_FOR_MOVE_ACKNOWLEDGEMENT";
        case RAD_CONTEXT_MENU_STATE_ENTITY_ATTACK:
            return "ENTITY_ATTACK";
        case RAD_CONTEXT_MENU_STATE_WAITING_FOR_ATTACK_ACKNOWLEDGEMENT:
            return "WAITING_FOR_ATTACK_ACKNOWLEDGEMENT";
        case RAD_CONTEXT_MENU_STATE_WAITING_FOR_SERVER_RESPONSE:
            return "WAITING_FOR_SERVER_RESPONSE";
    }
    return "?";
}
