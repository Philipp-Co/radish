#include <radish/events/event_manager.h>
#include <stdlib.h>
#include <stddef.h>


///
/// Die Struktur steht hier und nicht im Header: niemand sonst braucht sie, und was
/// niemand sieht, kann auch niemand an den Subscribe-Funktionen vorbei setzen.
///
struct RAD_NetEventManager
{
    RAD_NetEventsCommandResponseCallback_t command_response_events;
    RAD_NetEventsGameCallback_t game_events;
    RAD_NetEventsTileCallback_t tile_events;
};


static void RAD_DefaultOnNetCommandResponse(void *user_argument, const RAD_NetCommandResponse_t *response);

static void RAD_DefaultOnNetGameCreated(void *user_argument);
static void RAD_DefaultOnNetGameFinished(void *user_argument);
static void RAD_DefaultOnNetCurrentPlayer(void *user_argument, uint64_t user_id);
static void RAD_DefaultOnNetPlayers(void *user_argument, const uint64_t *user_ids, size_t number_of_users);
static void RAD_DefaultOnNetWorldSize(void *user_argument, uint32_t width, uint32_t height);
static void RAD_DefaultOnNetTiles(void *user_argument, const RAD_NetTile_t *tiles, size_t number_of_tiles);

static void RAD_DefaultOnNetTileCreated(void *user_argument, const RAD_NetTile_t *tile);
static void RAD_DefaultOnNetTileRemoved(void *user_argument, uint32_t x, uint32_t y);
static void RAD_DefaultOnNetTileChanged(void *user_argument, const RAD_NetTile_t *tile);


RAD_NetEventManager_t* RAD_CreateNetEventManager(void)
{
    RAD_NetEventManager_t *manager = malloc(sizeof(struct RAD_NetEventManager));
    if(manager == NULL)
    {
        return NULL;
    }

    *manager = (struct RAD_NetEventManager){
        .command_response_events = {
            .user_argument = NULL,
            .received = RAD_DefaultOnNetCommandResponse
        },
        .game_events = {
            .user_argument = NULL,
            .created = RAD_DefaultOnNetGameCreated,
            .finished = RAD_DefaultOnNetGameFinished,
            .current_player = RAD_DefaultOnNetCurrentPlayer,
            .players = RAD_DefaultOnNetPlayers,
            .world_size = RAD_DefaultOnNetWorldSize,
            .tiles = RAD_DefaultOnNetTiles
        },
        .tile_events = {
            .user_argument = NULL,
            .created = RAD_DefaultOnNetTileCreated,
            .removed = RAD_DefaultOnNetTileRemoved,
            .changed = RAD_DefaultOnNetTileChanged
        }
    };

    return manager;
}

void RAD_DestroyNetEventManager(RAD_NetEventManager_t **manager)
{
    free(*manager);
    *manager = NULL;
}


void RAD_NetEventManagerSubscribeToCommandResponseEvents(RAD_NetEventManager_t *manager, RAD_NetEventsCommandResponseCallback_t callbacks)
{
    manager->command_response_events = callbacks;
}

void RAD_NetEventManagerPublishCommandResponse(RAD_NetEventManager_t *manager, const RAD_NetCommandResponse_t *response)
{
    manager->command_response_events.received(manager->command_response_events.user_argument, response);
}

static void RAD_DefaultOnNetCommandResponse(void *user_argument, const RAD_NetCommandResponse_t *response)
{
    (void)user_argument;
    (void)response;
}


void RAD_NetEventManagerSubscribeToGameEvents(RAD_NetEventManager_t *manager, RAD_NetEventsGameCallback_t callbacks)
{
    manager->game_events = callbacks;
}

void RAD_NetEventManagerPublishGameCreated(RAD_NetEventManager_t *manager)
{
    manager->game_events.created(manager->game_events.user_argument);
}

void RAD_NetEventManagerPublishGameFinished(RAD_NetEventManager_t *manager)
{
    manager->game_events.finished(manager->game_events.user_argument);
}

void RAD_NetEventManagerPublishCurrentPlayer(RAD_NetEventManager_t *manager, uint64_t user_id)
{
    manager->game_events.current_player(manager->game_events.user_argument, user_id);
}

void RAD_NetEventManagerPublishPlayers(RAD_NetEventManager_t *manager, const uint64_t *user_ids, size_t number_of_users)
{
    manager->game_events.players(manager->game_events.user_argument, user_ids, number_of_users);
}

void RAD_NetEventManagerPublishWorldSize(RAD_NetEventManager_t *manager, uint32_t width, uint32_t height)
{
    manager->game_events.world_size(manager->game_events.user_argument, width, height);
}

void RAD_NetEventManagerPublishTiles(RAD_NetEventManager_t *manager, const RAD_NetTile_t *tiles, size_t number_of_tiles)
{
    manager->game_events.tiles(manager->game_events.user_argument, tiles, number_of_tiles);
}

static void RAD_DefaultOnNetGameCreated(void *user_argument)
{
    (void)user_argument;
}

static void RAD_DefaultOnNetGameFinished(void *user_argument)
{
    (void)user_argument;
}

static void RAD_DefaultOnNetCurrentPlayer(void *user_argument, uint64_t user_id)
{
    (void)user_argument;
    (void)user_id;
}

static void RAD_DefaultOnNetPlayers(void *user_argument, const uint64_t *user_ids, size_t number_of_users)
{
    (void)user_argument;
    (void)user_ids;
    (void)number_of_users;
}

static void RAD_DefaultOnNetWorldSize(void *user_argument, uint32_t width, uint32_t height)
{
    (void)user_argument;
    (void)width;
    (void)height;
}

static void RAD_DefaultOnNetTiles(void *user_argument, const RAD_NetTile_t *tiles, size_t number_of_tiles)
{
    (void)user_argument;
    (void)tiles;
    (void)number_of_tiles;
}

const char* RAD_NetTileTypeText(RAD_NetTileType_t type)
{
    switch(type)
    {
        case RAD_NET_TILE_TYPE_GROUND:  return "ground";
        case RAD_NET_TILE_TYPE_WATER:   return "water";
        case RAD_NET_TILE_TYPE_VOID:    return "void";
        case RAD_NET_TILE_TYPE_UNKNOWN: return "unbekannt";
    }
    return "unbekannt";
}


void RAD_NetEventManagerSubscribeToTileEvents(RAD_NetEventManager_t *manager, RAD_NetEventsTileCallback_t callbacks)
{
    manager->tile_events = callbacks;
}

void RAD_NetEventManagerPublishTileCreated(RAD_NetEventManager_t *manager, const RAD_NetTile_t *tile)
{
    manager->tile_events.created(manager->tile_events.user_argument, tile);
}

void RAD_NetEventManagerPublishTileRemoved(RAD_NetEventManager_t *manager, uint32_t x, uint32_t y)
{
    manager->tile_events.removed(manager->tile_events.user_argument, x, y);
}

void RAD_NetEventManagerPublishTileChanged(RAD_NetEventManager_t *manager, const RAD_NetTile_t *tile)
{
    manager->tile_events.changed(manager->tile_events.user_argument, tile);
}

static void RAD_DefaultOnNetTileCreated(void *user_argument, const RAD_NetTile_t *tile)
{
    (void)user_argument;
    (void)tile;
}

static void RAD_DefaultOnNetTileRemoved(void *user_argument, uint32_t x, uint32_t y)
{
    (void)user_argument;
    (void)x;
    (void)y;
}

static void RAD_DefaultOnNetTileChanged(void *user_argument, const RAD_NetTile_t *tile)
{
    (void)user_argument;
    (void)tile;
}
