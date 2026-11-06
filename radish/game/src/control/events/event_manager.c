#include <radish/game/control/events/event_manager.h>
#include <radish/game/game_definitions.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>


///
/// Die Struktur steht hier und nicht im Header -- dieselbe Ueberlegung wie bei
/// struct RAD_Control im Server: niemand sonst braucht sie, und was niemand sieht,
/// kann auch niemand an den Subscribe-Funktionen vorbei setzen.
///
// Die Liste ist oeffentlich, die Zahl der Mitspieler nicht -- beide muessen
// trotzdem gleich sein, sonst passte nicht jeder Mitspieler hinein.
_Static_assert(RAD_EVENT_MAX_RECIPIENTS == RAD_MAX_PLAYERS,
               "RAD_EVENT_MAX_RECIPIENTS muss RAD_MAX_PLAYERS sein");

///
/// Ein Abo je Spieler: wer, und was er bekommt.
///
typedef struct
{
    RAD_UserId_t user;
    RAD_EventsUnitChangedCallback_t callbacks;
} RAD_EventsUserSubscription_t;

struct RAD_EventManager
{
    RAD_EventsUnitChangedCallback_t unit_changed_events;
    ///
    /// Die Abos je Spieler, dicht von vorn belegt -- "number_of_user_unit_events"
    /// Eintraege, in der Reihenfolge, in der sie kamen. Gesucht wird linear: es
    /// sind hoechstens RAD_EVENT_MAX_RECIPIENTS.
    ///
    RAD_EventsUserSubscription_t user_unit_events[RAD_EVENT_MAX_RECIPIENTS];
    int32_t number_of_user_unit_events;
    RAD_EventsTileChangedCallback_t tile_changed_events;
    RAD_EventsMouseCallbacks_t mouse_events;
    RAD_EventsTurnCallback_t turn_events;
    RAD_EventsGameCallback_t game_events;
};


static void RAD_DefaultOnTileAddedToGame(void *user_argument, const RAD_Tile_t *tile);
static void RAD_DefaultOnTileRemovedFromGame(void *user_argument, const RAD_Tile_t *tile);
static void RAD_DefaultOnTileStateChanged(void *user_argument, const RAD_Tile_t *tile);

static void RAD_DefaultOnMouseMove(void *user_argument, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y);
static void RAD_DefaultOnMousePressed(void *user_argument, int32_t x, int32_t y);
static void RAD_DefaultOnMouseReleased(void *user_argument, int32_t x, int32_t y);

static void RAD_DefaultUnit(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y);
static void RAD_DefaultUnitMove(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result);
static void RAD_DefaultUnitAttack(void *user_argument, const RAD_AttackResult_t *result);
static void RAD_DefaultUnitLost(void *user_argument, RAD_UnitId_t unit, int32_t x, int32_t y);
static RAD_EventsUnitChangedCallback_t RAD_EventsUnitWithDefaults(RAD_EventsUnitChangedCallback_t callbacks);
static const RAD_EventsUnitChangedCallback_t* RAD_EventManagerUnitEventsOf(const RAD_EventManager_t *manager, RAD_UserId_t user);

static void RAD_DefaultOnTurnChanged(void *user_argument, RAD_UserId_t current_user);

static void RAD_DefaultOnGameFinished(void *user_argument, RAD_UserId_t winner, RAD_GameEndReason_t reason);


RAD_EventManager_t* RAD_CreateEventManager(void)
{
    RAD_EventManager_t *manager = malloc(sizeof(struct RAD_EventManager));
    if(manager == NULL)
    {
        return NULL;
    }

    *manager = (struct RAD_EventManager){
        .tile_changed_events = {
            .user_argument = NULL,
            .added=RAD_DefaultOnTileAddedToGame,
            .removed=RAD_DefaultOnTileRemovedFromGame,
            .changed=RAD_DefaultOnTileStateChanged
        },
        .mouse_events = {
            .user_argument = NULL,
            .move=RAD_DefaultOnMouseMove,
            .pressed=RAD_DefaultOnMousePressed,
            .released=RAD_DefaultOnMouseReleased
        },
        .unit_changed_events = {
            .user_argument = NULL,
            .spawned = RAD_DefaultUnit,
            .destroyed = RAD_DefaultUnit,
            .moved = RAD_DefaultUnitMove,
            .attacked = RAD_DefaultUnitAttack,
            .sighted = RAD_DefaultUnit,
            .lost = RAD_DefaultUnitLost
        },
        .number_of_user_unit_events = 0,
        .turn_events = {
            .user_argument = NULL,
            .changed = RAD_DefaultOnTurnChanged
        },
        .game_events = {
            .user_argument = NULL,
            .finished = RAD_DefaultOnGameFinished
        }
    };

    return manager;
}

void RAD_DestroyEventManager(RAD_EventManager_t **manager)
{
    free(*manager);
    *manager = NULL;
}

void RAD_EventManagerSubscribeToTileEvents(RAD_EventManager_t *manager, RAD_EventsTileChangedCallback_t callbacks)
{
    manager->tile_changed_events = callbacks;
}

void RAD_EventManagerPublishTileAddedToGameEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile)
{
    manager->tile_changed_events.added(manager->tile_changed_events.user_argument, tile);
}

void RAD_EventManagerPublishTileRemovedFromGameEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile)
{
    manager->tile_changed_events.removed(manager->tile_changed_events.user_argument, tile);
}

void RAD_EventManagerPublishTileStateChangeEvent(RAD_EventManager_t *manager, const RAD_Tile_t *tile)
{
    manager->tile_changed_events.changed(manager->tile_changed_events.user_argument, tile);
}


static void RAD_DefaultOnTileAddedToGame(void *user_argument, const RAD_Tile_t *tile)
{
    (void)user_argument;
    (void)tile;
}

static void RAD_DefaultOnTileRemovedFromGame(void *user_argument, const RAD_Tile_t *tile)
{
    (void)user_argument;
    (void)tile;
}

static void RAD_DefaultOnTileStateChanged(void *user_argument, const RAD_Tile_t *tile)
{
    (void)user_argument;
    (void)tile;
}


void RAD_EventManagerSubscribeToMouseEvents(RAD_EventManager_t *manager, RAD_EventsMouseCallbacks_t callbacks)
{
    manager->mouse_events = callbacks;
}

void RAD_EventManagerPublishMouseMoved(RAD_EventManager_t *manager, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y)
{
    manager->mouse_events.move(manager->mouse_events.user_argument, new_x, new_y, old_x, old_y);
}

void RAD_EventManagerPublishMousePressed(RAD_EventManager_t *manager, int32_t x, int32_t y)
{
    manager->mouse_events.pressed(manager->mouse_events.user_argument, x, y);
}

void RAD_EventManagerPublishMouseReleased(RAD_EventManager_t *manager, int32_t x, int32_t y)
{
    manager->mouse_events.released(manager->mouse_events.user_argument, x, y);
}

static void RAD_DefaultOnMouseMove(void *user_argument, int32_t new_x, int32_t new_y, int32_t old_x, int32_t old_y)
{
    (void)user_argument;
    (void)new_x;
    (void)new_y;
    (void)old_x;
    (void)old_y;
}

static void RAD_DefaultOnMousePressed(void *user_argument, int32_t x, int32_t y)
{
    (void)user_argument;
    (void)x;
    (void)y;
}

static void RAD_DefaultOnMouseReleased(void *user_argument, int32_t x, int32_t y)
{
    (void)user_argument;
    (void)x;
    (void)y;
}

bool RAD_RecipientsAdd(RAD_Recipients_t *recipients, RAD_UserId_t user)
{
    if((recipients == NULL) || (user == RAD_USER_NONE))
    {
        return false;
    }
    if(RAD_RecipientsContains(recipients, user))
    {
        return true;
    }
    if(recipients->number_of_users >= RAD_EVENT_MAX_RECIPIENTS)
    {
        return false;
    }
    recipients->users[recipients->number_of_users++] = user;
    return true;
}

bool RAD_RecipientsContains(const RAD_Recipients_t *recipients, RAD_UserId_t user)
{
    if(recipients == NULL)
    {
        return false;
    }
    for(int32_t i = 0; i < recipients->number_of_users; ++i)
    {
        if(recipients->users[i] == user)
        {
            return true;
        }
    }
    return false;
}

void RAD_EventManagerSubscribeToUnitEvents(RAD_EventManager_t *manager, RAD_EventsUnitChangedCallback_t callbacks)
{
    manager->unit_changed_events = RAD_EventsUnitWithDefaults(callbacks);
}

bool RAD_EventManagerSubscribeToUnitEventsOf(RAD_EventManager_t *manager, RAD_UserId_t user, RAD_EventsUnitChangedCallback_t callbacks)
{
    if(user == RAD_USER_NONE)
    {
        return false;
    }

    // Wer schon abonniert hat, wird ersetzt und behaelt seinen Platz.
    for(int32_t i = 0; i < manager->number_of_user_unit_events; ++i)
    {
        if(manager->user_unit_events[i].user == user)
        {
            manager->user_unit_events[i].callbacks = RAD_EventsUnitWithDefaults(callbacks);
            return true;
        }
    }

    if(manager->number_of_user_unit_events >= RAD_EVENT_MAX_RECIPIENTS)
    {
        return false;
    }
    manager->user_unit_events[manager->number_of_user_unit_events++] = (RAD_EventsUserSubscription_t){
        .user = user,
        .callbacks = RAD_EventsUnitWithDefaults(callbacks)
    };
    return true;
}

void RAD_EventManagerUnsubscribeUser(RAD_EventManager_t *manager, RAD_UserId_t user)
{
    for(int32_t i = 0; i < manager->number_of_user_unit_events; ++i)
    {
        if(manager->user_unit_events[i].user != user)
        {
            continue;
        }
        // Dicht halten: die Nachfolger ruecken auf, die Reihenfolge bleibt.
        for(int32_t j = i + 1; j < manager->number_of_user_unit_events; ++j)
        {
            manager->user_unit_events[j - 1] = manager->user_unit_events[j];
        }
        manager->number_of_user_unit_events--;
        return;
    }
}

///
/// Jede Publish-Funktion unten geht gleich vor: erst der Abonnent ohne
/// Empfaenger -- ausser bei einer weiteren Fassung (RAD_Recipients_t.variant) --,
/// dann je Eintrag der Liste der Abonnent dieses Benutzers, wenn er einen hat. Ein Makro und keine Funktion, weil die Callbacks verschiedene
/// Signaturen haben; "callback" ist das Feld der Gruppe, der Rest die Argumente
/// nach user_argument.
///
#define RAD_EVENT_MANAGER_PUBLISH_UNIT(manager, recipients, callback, ...)                          \
    do                                                                                             \
    {                                                                                              \
        if(((recipients) == NULL) || !(recipients)->variant)                                       \
        {                                                                                          \
            (manager)->unit_changed_events.callback((manager)->unit_changed_events.user_argument, \
                                                    __VA_ARGS__);                                  \
        }                                                                                          \
        const int32_t number_of_recipients = ((recipients) == NULL) ? 0 : (recipients)->number_of_users; \
        for(int32_t i = 0; i < number_of_recipients; ++i)                                          \
        {                                                                                          \
            const RAD_EventsUnitChangedCallback_t *of =                                            \
                RAD_EventManagerUnitEventsOf((manager), (recipients)->users[i]);                   \
            if(of != NULL)                                                                         \
            {                                                                                      \
                of->callback(of->user_argument, __VA_ARGS__);                                      \
            }                                                                                      \
        }                                                                                          \
    } while(0)

void RAD_EventManagerPublishUnitSpawned(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                        const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    RAD_EVENT_MANAGER_PUBLISH_UNIT(manager, recipients, spawned, unit, x, y);
}

void RAD_EventManagerPublishUnitDestroyed(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                          const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    RAD_EVENT_MANAGER_PUBLISH_UNIT(manager, recipients, destroyed, unit, x, y);
}

void RAD_EventManagerPublishUnitMoved(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                      const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    RAD_EVENT_MANAGER_PUBLISH_UNIT(manager, recipients, moved, unit, path, result);
}

void RAD_EventManagerPublishUnitAttacked(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                         const RAD_AttackResult_t *result)
{
    RAD_EVENT_MANAGER_PUBLISH_UNIT(manager, recipients, attacked, result);
}

void RAD_EventManagerPublishUnitSighted(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                        const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    RAD_EVENT_MANAGER_PUBLISH_UNIT(manager, recipients, sighted, unit, x, y);
}

void RAD_EventManagerPublishUnitLost(RAD_EventManager_t *manager, const RAD_Recipients_t *recipients,
                                     RAD_UnitId_t unit, int32_t x, int32_t y)
{
    RAD_EVENT_MANAGER_PUBLISH_UNIT(manager, recipients, lost, unit, x, y);
}

/// Das Abo von "user", NULL, wenn er keins hat.
static const RAD_EventsUnitChangedCallback_t* RAD_EventManagerUnitEventsOf(const RAD_EventManager_t *manager, RAD_UserId_t user)
{
    for(int32_t i = 0; i < manager->number_of_user_unit_events; ++i)
    {
        if(manager->user_unit_events[i].user == user)
        {
            return &manager->user_unit_events[i].callbacks;
        }
    }
    return NULL;
}

///
/// Was ein Abonnent der Einheiten nicht gesetzt hat, wird die Voreinstellung --
/// so ruft keine Publish-Funktion je NULL auf, auch bei einem Abonnenten, der die
/// Gruppe kannte, bevor sie "sighted" und "lost" hatte.
///
static RAD_EventsUnitChangedCallback_t RAD_EventsUnitWithDefaults(RAD_EventsUnitChangedCallback_t callbacks)
{
    if(callbacks.spawned == NULL)   callbacks.spawned = RAD_DefaultUnit;
    if(callbacks.destroyed == NULL) callbacks.destroyed = RAD_DefaultUnit;
    if(callbacks.moved == NULL)     callbacks.moved = RAD_DefaultUnitMove;
    if(callbacks.attacked == NULL)  callbacks.attacked = RAD_DefaultUnitAttack;
    if(callbacks.sighted == NULL)   callbacks.sighted = RAD_DefaultUnit;
    if(callbacks.lost == NULL)      callbacks.lost = RAD_DefaultUnitLost;
    return callbacks;
}

static void RAD_DefaultUnitLost(void *user_argument, RAD_UnitId_t unit, int32_t x, int32_t y)
{
    (void)user_argument;
    (void)unit;
    (void)x;
    (void)y;
}

static void RAD_DefaultUnit(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)user_argument;
    (void)unit;
    (void)x;
    (void)y;
}

static void RAD_DefaultUnitMove(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    (void)user_argument;
    (void)unit;
    (void)path;
    (void)result;
}

static void RAD_DefaultUnitAttack(void *user_argument, const RAD_AttackResult_t *result)
{
    (void)user_argument;
    (void)result;
}

void RAD_EventManagerSubscribeToTurnEvents(RAD_EventManager_t *manager, RAD_EventsTurnCallback_t callbacks)
{
    manager->turn_events = callbacks;
}

void RAD_EventManagerPublishTurnChanged(RAD_EventManager_t *manager, RAD_UserId_t current_user)
{
    manager->turn_events.changed(manager->turn_events.user_argument, current_user);
}

static void RAD_DefaultOnTurnChanged(void *user_argument, RAD_UserId_t current_user)
{
    (void)user_argument;
    (void)current_user;
}

void RAD_EventManagerSubscribeToGameEvents(RAD_EventManager_t *manager, RAD_EventsGameCallback_t callbacks)
{
    manager->game_events = callbacks;
}

void RAD_EventManagerPublishGameFinished(RAD_EventManager_t *manager, RAD_UserId_t winner, RAD_GameEndReason_t reason)
{
    manager->game_events.finished(manager->game_events.user_argument, winner, reason);
}

static void RAD_DefaultOnGameFinished(void *user_argument, RAD_UserId_t winner, RAD_GameEndReason_t reason)
{
    (void)user_argument;
    (void)winner;
    (void)reason;
}
