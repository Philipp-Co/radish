#ifndef __RAD_IO_NET_EVENT_HANDLER_H__
#define __RAD_IO_NET_EVENT_HANDLER_H__

#include <stddef.h>
#include <stdint.h>
#include <radish/io/net_session.h>
#include <radish/io/net_types.h>
#include <radish/view/user_input.h>
#include <radish/model/world.h>

///
/// io/net_event_handler/ -- die Abonnenten des Net-Event-Managers
/// (events/event_manager.h), je Ereignis eine Datei. Der Manager hat je Gruppe
/// nur einen Abonnenten -- hier wird deshalb verteilt: ins Log und in den
/// Weltzustand (world).
///
/// **In die Darstellung schreiben sie nicht.** Was sich aendert, erfaehrt sie
/// vom Model (model/events/observable.h, view/iso_map_view.h).
///
/// Alle Handler bekommen als "user_argument" einen
/// RAD_IoNetEventHandlerContext_t. main.c legt ihn an und registriert die
/// Handler damit.
///

typedef struct
{
    // Worauf die Handler wirken. Die Objekte gehoeren main.c.
    RAD_ClientWorld_t *world;
    RAD_IoUserInput_t *user_input;

    // Was ueber die Verbindung bekannt ist -- hier nur gelesen, geschrieben wird
    // es beim Senden (io/net_request.h) und von main.c.
    const RAD_IoNetSession_t *session;
} RAD_IoNetEventHandlerContext_t;

// Antwort auf ein eigenes Kommando (command_response.c).
void RAD_IoNetOnCommandResponse(void *user_argument, const RAD_NetCommandResponse_t *response);

// Spielereignisse (game_created.c, game_finished.c, current_player.c, players.c,
// world_size.c, tiles.c, reserve_unit.c).
void RAD_IoNetOnGameCreated(void *user_argument);
void RAD_IoNetOnGameFinished(void *user_argument);
void RAD_IoNetOnCurrentPlayer(void *user_argument, uint64_t user_id);
void RAD_IoNetOnPlayers(void *user_argument, const uint64_t *user_ids, size_t number_of_users);
void RAD_IoNetOnWorldSize(void *user_argument, uint32_t width, uint32_t height);
void RAD_IoNetOnTiles(void *user_argument, const RAD_NetTile_t *tiles, size_t number_of_tiles);
void RAD_IoNetOnReserveUnit(void *user_argument, const RAD_NetReserveUnit_t *unit);

// Tile-Ereignisse (tile_created.c, tile_removed.c, tile_changed.c).
void RAD_IoNetOnTileCreated(void *user_argument, const RAD_NetTile_t *tile);
void RAD_IoNetOnTileRemoved(void *user_argument, uint32_t x, uint32_t y);
void RAD_IoNetOnTileChanged(void *user_argument, const RAD_NetTile_t *tile);

#endif
