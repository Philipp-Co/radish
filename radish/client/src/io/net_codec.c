#include <radish/io/net_codec.h>

#include <stdlib.h>
#include <string.h>

// Generierter Code aus protobuf/*.proto (protobuf/CMakeLists.txt, Ziel
// radish_protobuf). Bleibt auf diese Datei beschraenkt -- net_codec.h zeigt
// nach aussen nur die Typen aus io/net_types.h, die Verteilung aus
// events/event_manager.h und die eigene RAD_NetCodecResult_t.
#include "message.pb-c.h"


const char* RAD_NetCodecResultText(RAD_NetCodecResult_t result)
{
    switch(result)
    {
        case RAD_NET_CODEC_OK:                             return "in Ordnung";
        case RAD_NET_CODEC_ERROR_UNSUPPORTED_COMMAND_TYPE:  return "Kommandoart im Client nicht abgebildet";
        case RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT:        return "unmoegliche Anzahl von Schritten";
        case RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL:          return "Puffer zu klein fuer die gepackte Nachricht";
        case RAD_NET_CODEC_ERROR_DECODE_FAILED:             return "Bytes liessen sich nicht als NetServerMessage lesen";
        case RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE:        return "NetServerMessage ist strukturell nicht das, was ihr Zweig behauptet";
        default:                                            return "unbekanntes Ergebnis";
    }
}

RAD_NetCodecResult_t RAD_NetEncodeMoveRequest(const RAD_NetMoveRequest_t *request,
                                               uint8_t *buffer,
                                               size_t buffer_size,
                                               size_t *out_length)
{
    const int8_t number_of_steps = request->path.number_of_steps;

    // Das erste Feld ist Teil des Pfades und kein Schritt fuer sich, ein Pfad
    // braucht also mindestens zwei (net_types.h).
    if((number_of_steps < 2) || (number_of_steps > RAD_NET_PATH_MAX_STEPS))
    {
        return RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT;
    }

    // Nur die tatsaechlich genutzten Felder fahren mit: Protobuf traegt seine
    // eigene Laenge (n_steps).
    NetMoveCommandStep steps[RAD_NET_PATH_MAX_STEPS];
    NetMoveCommandStep *step_pointers[RAD_NET_PATH_MAX_STEPS];
    for(int8_t i = 0; i < number_of_steps; ++i)
    {
        net_move_command_step__init(&steps[i]);
        steps[i].x = (uint32_t)request->path.steps_to[i].x;
        steps[i].y = (uint32_t)request->path.steps_to[i].y;
        step_pointers[i] = &steps[i];
    }

    NetMoveCommand move_command = NET_MOVE_COMMAND__INIT;
    // uint64 -> uint32: siehe die Anmerkung in net_codec.h zu Sequenznummer
    // und Absender.
    move_command.user_id = (uint32_t)request->user;
    move_command.entity_id = (uint32_t)request->entity;
    move_command.n_steps = (size_t)number_of_steps;
    move_command.steps = step_pointers;

    NetCommandRequest command_request = NET_COMMAND_REQUEST__INIT;
    command_request.id = (uint32_t)request->sequence;
    command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_MOVE;
    command_request.move = &move_command;

    NetUserRequest user_request = NET_USER_REQUEST__INIT;
    user_request.data_case = NET_USER_REQUEST__DATA_COMMAND_REQUEST;
    user_request.command_request = &command_request;

    const size_t packed_size = net_user_request__get_packed_size(&user_request);
    if(packed_size > buffer_size)
    {
        return RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL;
    }

    *out_length = net_user_request__pack(&user_request, buffer);
    return RAD_NET_CODEC_OK;
}

RAD_NetCodecResult_t RAD_NetEncodeDiscover(uint32_t x,
                                            uint32_t y,
                                            uint32_t w,
                                            uint32_t h,
                                            uint8_t *buffer,
                                            size_t buffer_size,
                                            size_t *out_length)
{
    NetDiscoverRequest discover = NET_DISCOVER_REQUEST__INIT;
    discover.x = x;
    discover.y = y;
    discover.w = w;
    discover.h = h;

    NetUserRequest user_request = NET_USER_REQUEST__INIT;
    user_request.data_case = NET_USER_REQUEST__DATA_DISCOVER_REQUEST;
    user_request.discover_request = &discover;

    const size_t packed_size = net_user_request__get_packed_size(&user_request);
    if(packed_size > buffer_size)
    {
        return RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL;
    }

    *out_length = net_user_request__pack(&user_request, buffer);
    return RAD_NET_CODEC_OK;
}

static void RAD_NetFillTile(RAD_NetTile_t *out, const NetTile *tile);
static RAD_NetCodecResult_t RAD_NetDispatchTiles(RAD_NetEventManager_t *events, const NetTilesEvent *tiles_event);

static RAD_NetCodecResult_t RAD_NetDispatchCommandResponse(RAD_NetEventManager_t *events, const NetCommandResponse *command_response)
{
    // Ohne eingebettetes NetCommandRequest ist das strukturell keine Antwort
    // auf ein Kommando.
    if((command_response == NULL) || (command_response->command == NULL))
    {
        return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }

    const NetCommandRequest *request = command_response->command;

    // Nur der Zug ist im Client abgebildet -- ein shoot-Zweig oder gar keiner
    // (commands_case NOT_SET) laesst sich nicht in eine
    // RAD_NetCommandResponse_t uebersetzen (net_codec.h).
    if((request->commands_case != NET_COMMAND_REQUEST__COMMANDS_MOVE) ||
       (request->move == NULL))
    {
        return RAD_NET_CODEC_ERROR_UNSUPPORTED_COMMAND_TYPE;
    }

    const NetMoveCommand *move = request->move;
    const size_t number_of_steps = move->n_steps;

    // Dieselbe Grenze wie beim Encodieren.
    if((number_of_steps < 2) || (number_of_steps > RAD_NET_PATH_MAX_STEPS))
    {
        return RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT;
    }

    RAD_NetCommandResponse_t response;
    memset(&response, 0, sizeof(response));

    // uint32 -> uint64: erweitert, nicht abgeschnitten -- die Umkehrung der
    // Anmerkung beim Encodieren.
    response.sequence = (RAD_NetSequence_t)request->id;
    response.user = (RAD_NetUserId_t)move->user_id;
    response.success = command_response->success;

    response.entity = (RAD_NetEntityId_t)move->entity_id;
    response.path.number_of_steps = (int8_t)number_of_steps;
    for(size_t i = 0; i < number_of_steps; ++i)
    {
        response.path.steps_to[i].x = (int16_t)move->steps[i]->x;
        response.path.steps_to[i].y = (int16_t)move->steps[i]->y;
    }

    RAD_NetEventManagerPublishCommandResponse(events, &response);
    return RAD_NET_CODEC_OK;
}

static RAD_NetCodecResult_t RAD_NetDispatchGameEvent(RAD_NetEventManager_t *events, const NetGameEvent *game_event)
{
    if(game_event == NULL)
    {
        return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }

    switch(game_event->event_case)
    {
        case NET_GAME_EVENT__EVENT_CREATED:
            RAD_NetEventManagerPublishGameCreated(events);
            return RAD_NET_CODEC_OK;

        case NET_GAME_EVENT__EVENT_FINISHED:
            RAD_NetEventManagerPublishGameFinished(events);
            return RAD_NET_CODEC_OK;

        case NET_GAME_EVENT__EVENT_CURRENT_PLAYER:
            if(game_event->current_player == NULL)
            {
                return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            }
            RAD_NetEventManagerPublishCurrentPlayer(events, game_event->current_player->user_id);
            return RAD_NET_CODEC_OK;

        case NET_GAME_EVENT__EVENT_PLAYERS:
            if(game_event->players == NULL)
            {
                return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            }
            // Der Zeiger gehoert der entpackten Nachricht und wird mit ihr
            // freigegeben -- deshalb gilt er nur waehrend des Callbacks
            // (events/event_manager.h).
            RAD_NetEventManagerPublishPlayers(events, game_event->players->user_ids, game_event->players->n_user_ids);
            return RAD_NET_CODEC_OK;

        case NET_GAME_EVENT__EVENT_WORLD_SIZE:
            if(game_event->world_size == NULL)
            {
                return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            }
            RAD_NetEventManagerPublishWorldSize(events, game_event->world_size->width, game_event->world_size->height);
            return RAD_NET_CODEC_OK;

        case NET_GAME_EVENT__EVENT_TILES:
            return RAD_NetDispatchTiles(events, game_event->tiles);

        case NET_GAME_EVENT__EVENT__NOT_SET:
        default:
            return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }
}

///
/// NetTile (tile.proto) -> RAD_NetTile_t (io/net_types.h). Eine reine
/// Feldkopie, das Wire-Enum eingeschlossen -- NET_TILE_TYPE__* und
/// RAD_NET_TILE_TYPE_* zaehlen absichtlich gleich (0 unbekannt, 1 ground,
/// 2 water, 3 void), trotzdem uebersetzt ein switch statt eines nackten Casts.
/// Der default faengt Wire-Werte ab, die dieser Client noch nicht kennt: ein
/// neuerer Server darf einen Typ schicken, den es hier noch nicht gibt, und der
/// ist dann "unbekannt" und kein Fehler.
///
static void RAD_NetFillTile(RAD_NetTile_t *out, const NetTile *tile)
{
    out->x = tile->x;
    out->y = tile->y;
    out->z = tile->z;
    out->entity_id = (RAD_NetEntityId_t)tile->entity_id;

    switch(tile->type)
    {
        case NET_TILE_TYPE__TILE_TYPE_GROUND: out->type = RAD_NET_TILE_TYPE_GROUND;   break;
        case NET_TILE_TYPE__TILE_TYPE_WATER:  out->type = RAD_NET_TILE_TYPE_WATER;    break;
        case NET_TILE_TYPE__TILE_TYPE_VOID:   out->type = RAD_NET_TILE_TYPE_VOID;     break;
        case NET_TILE_TYPE__TILE_TYPE_UNKNOWN:
        default:                              out->type = RAD_NET_TILE_TYPE_UNKNOWN;  break;
    }
}

///
/// NetTilesEvent (game.proto) -> eine Liste RAD_NetTile_t, einmal
/// veroeffentlicht. Die Liste liegt auf dem Heap und nicht auf dem Stapel: wie
/// viele Felder in einer Nachricht stehen, legt der Server fest und nicht dieser
/// Client -- heute eines, spaeter vielleicht eine ganze Welt. Freigegeben wird
/// nach dem Callback, deshalb gilt der Zeiger nur waehrend er laeuft
/// (events/event_manager.h).
///
static RAD_NetCodecResult_t RAD_NetDispatchTiles(RAD_NetEventManager_t *events, const NetTilesEvent *tiles_event)
{
    if(tiles_event == NULL)
    {
        return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }

    const size_t number_of_tiles = tiles_event->n_tiles;
    if(number_of_tiles == 0)
    {
        RAD_NetEventManagerPublishTiles(events, NULL, 0);
        return RAD_NET_CODEC_OK;
    }

    RAD_NetTile_t *tiles = malloc(number_of_tiles * sizeof(RAD_NetTile_t));
    if(tiles == NULL)
    {
        return RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL;
    }

    for(size_t i = 0; i < number_of_tiles; ++i)
    {
        if(tiles_event->tiles[i] == NULL)
        {
            free(tiles);
            return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
        }
        RAD_NetFillTile(&tiles[i], tiles_event->tiles[i]);
    }

    RAD_NetEventManagerPublishTiles(events, tiles, number_of_tiles);

    free(tiles);
    return RAD_NET_CODEC_OK;
}

static RAD_NetCodecResult_t RAD_NetDispatchTileEvent(RAD_NetEventManager_t *events, const NetTileEvent *tile_event)
{
    if(tile_event == NULL)
    {
        return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }

    RAD_NetTile_t tile;

    switch(tile_event->event_case)
    {
        case NET_TILE_EVENT__EVENT_CREATED:
            if((tile_event->created == NULL) || (tile_event->created->tile == NULL))
            {
                return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            }
            RAD_NetFillTile(&tile, tile_event->created->tile);
            RAD_NetEventManagerPublishTileCreated(events, &tile);
            return RAD_NET_CODEC_OK;

        case NET_TILE_EVENT__EVENT_REMOVED:
            if(tile_event->removed == NULL)
            {
                return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            }
            RAD_NetEventManagerPublishTileRemoved(events, tile_event->removed->x, tile_event->removed->y);
            return RAD_NET_CODEC_OK;

        case NET_TILE_EVENT__EVENT_CHANGED:
            if((tile_event->changed == NULL) || (tile_event->changed->tile == NULL))
            {
                return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            }
            RAD_NetFillTile(&tile, tile_event->changed->tile);
            RAD_NetEventManagerPublishTileChanged(events, &tile);
            return RAD_NET_CODEC_OK;

        case NET_TILE_EVENT__EVENT__NOT_SET:
        default:
            return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }
}

static RAD_NetCodecResult_t RAD_NetDispatchEvent(RAD_NetEventManager_t *events, const NetEvent *event)
{
    if(event == NULL)
    {
        return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }

    switch(event->event_case)
    {
        case NET_EVENT__EVENT_GAME: return RAD_NetDispatchGameEvent(events, event->game);
        case NET_EVENT__EVENT_TILE: return RAD_NetDispatchTileEvent(events, event->tile);
        case NET_EVENT__EVENT__NOT_SET:
        default:                    return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }
}

RAD_NetCodecResult_t RAD_NetDispatchServerMessage(RAD_NetEventManager_t *events,
                                                   const uint8_t *data,
                                                   size_t length)
{
    // NULL als Allocator: protobuf-c greift dann auf malloc/free zurueck
    // (protobuf-c.h). Eigene Allokatoren braeuchte es erst, wenn der Client
    // eigene Speicherverwaltung mitbringt -- das tut er an keiner anderen
    // Stelle.
    NetServerMessage *message = net_server_message__unpack(NULL, length, data);
    if(message == NULL)
    {
        return RAD_NET_CODEC_ERROR_DECODE_FAILED;
    }

    RAD_NetCodecResult_t result;
    switch(message->data_case)
    {
        case NET_SERVER_MESSAGE__DATA_COMMAND_RESPONSE:
            result = RAD_NetDispatchCommandResponse(events, message->command_response);
            break;

        case NET_SERVER_MESSAGE__DATA_EVENT:
            result = RAD_NetDispatchEvent(events, message->event);
            break;

        case NET_SERVER_MESSAGE__DATA__NOT_SET:
        default:
            result = RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            break;
    }

    // Erst hier freigeben: alle drei Dispatch-Zweige lesen nur aus "message"
    // und veroeffentlichen dabei Kopien (RAD_NetFillTile, die Feldkopien in
    // RAD_NetDispatchCommandResponse) -- danach wird nichts davon mehr
    // gebraucht.
    net_server_message__free_unpacked(message, NULL);

    return result;
}
