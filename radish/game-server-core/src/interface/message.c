#include <radish/server/interface/message.h>

#include <string.h>

// Generierter Code aus protobuf/*.proto (protobuf/CMakeLists.txt, Ziel
// radish_protobuf). Bleibt auf diese Datei beschraenkt -- message.h zeigt nach
// aussen nur RAD_Command_t/RAD_CommandResponse_t und die eigene
// RAD_NetCodecResult_t, keinen generierten NetXxx-Typ und kein protobuf-c-
// Symbol.
#include "message.pb-c.h"


const char* RAD_NetCodecResultText(RAD_NetCodecResult_t result)
{
    switch(result)
    {
        case RAD_NET_CODEC_OK:                             return "in Ordnung";
        case RAD_NET_CODEC_ERROR_UNSUPPORTED_COMMAND_TYPE:  return "Kommandoart nicht in command.proto abgebildet";
        case RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT:        return "unmoegliche Anzahl von Schritten";
        case RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL:          return "Puffer zu klein fuer die gepackte Nachricht";
        case RAD_NET_CODEC_ERROR_DECODE_FAILED:             return "Bytes liessen sich nicht als NetUserRequest lesen";
        case RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE:        return "NetUserRequest ist strukturell nicht das, was ihr Zweig behauptet";
        default:                                            return "unbekanntes Ergebnis";
    }
}

RAD_NetCodecResult_t RAD_ParseCommandFromMessage(const uint8_t *message, uint16_t size, RAD_Command_t *out_command)
{
    // NULL als Allocator: protobuf-c greift dann auf malloc/free zurueck
    // (protobuf-c.h), wie beim Client (net_codec.c dort).
    NetUserRequest *request = net_user_request__unpack(NULL, size, message);
    if(request == NULL)
    {
        return RAD_NET_CODEC_ERROR_DECODE_FAILED;
    }

    if((request->data_case != NET_USER_REQUEST__DATA_COMMAND_REQUEST) || (request->command_request == NULL))
    {
        net_user_request__free_unpacked(request, NULL);
        return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }

    const NetCommandRequest *command_request = request->command_request;

    // Nur move_entity ist gegenwaertig abgebildet -- ein shoot-Zweig oder gar
    // keiner (commands_case NOT_SET) ist damit keine Nachricht, die sich in ein
    // RAD_Command_t uebersetzen liesse (message.h).
    if((command_request->commands_case != NET_COMMAND_REQUEST__COMMANDS_MOVE) || (command_request->move == NULL))
    {
        net_user_request__free_unpacked(request, NULL);
        return RAD_NET_CODEC_ERROR_UNSUPPORTED_COMMAND_TYPE;
    }

    const NetMoveCommand *move = command_request->move;
    const size_t number_of_steps = move->n_steps;

    // Dieselbe Grenze wie beim Client (net_codec.c dort) und beim alten Codec.
    if((number_of_steps < 2) || (number_of_steps > RAD_PATH_MAX_STEPS))
    {
        net_user_request__free_unpacked(request, NULL);
        return RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT;
    }

    memset(out_command, 0, sizeof(*out_command));

    out_command->header.type = RAD_COMMAND_TYPE_MOVE_ENTITY;
    // uint32 -> uint64: erweitert, nicht abgeschnitten -- die Umkehrung der
    // Anmerkung beim Client zum Encodieren (net_codec.h dort).
    out_command->header.sequence = (RAD_CommandSequence_t)command_request->id;
    out_command->header.user = (RAD_UserId_t)move->user_id;

    out_command->command.move_entity.entity = (RAD_EntityId_t)move->entity_id;
    out_command->command.move_entity.path.number_of_steps = (int8_t)number_of_steps;
    for(size_t i = 0; i < number_of_steps; ++i)
    {
        out_command->command.move_entity.path.steps_to[i].x = (int16_t)move->steps[i]->x;
        out_command->command.move_entity.path.steps_to[i].y = (int16_t)move->steps[i]->y;
    }
    // Die ungenutzten Plaetze bleiben genullt (memset oben) -- wie beim alten
    // Codec, dasselbe Kommando ergibt so immer dieselbe Struktur.

    net_user_request__free_unpacked(request, NULL);
    return RAD_NET_CODEC_OK;
}

RAD_NetCodecResult_t RAD_ParseDiscoverFromMessage(const uint8_t *message, uint16_t size, RAD_DiscoverRequest_t *out_request)
{
    NetUserRequest *request = net_user_request__unpack(NULL, size, message);
    if(request == NULL)
    {
        return RAD_NET_CODEC_ERROR_DECODE_FAILED;
    }

    if((request->data_case != NET_USER_REQUEST__DATA_DISCOVER_REQUEST) || (request->discover_request == NULL))
    {
        net_user_request__free_unpacked(request, NULL);
        return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }

    const NetDiscoverRequest *discover = request->discover_request;
    out_request->x = discover->x;
    out_request->y = discover->y;
    out_request->w = discover->w;
    out_request->h = discover->h;

    net_user_request__free_unpacked(request, NULL);
    return RAD_NET_CODEC_OK;
}

static NetTileType RAD_NetTileTypeFromTileType(RAD_TileType_t type);

///
/// Das gemeinsame Ende der Spiel-Ereignisse: "game_event" in NetEvent und
/// NetServerMessage einwickeln und packen.
///
static RAD_NetCodecResult_t RAD_PackGameEvent(NetGameEvent *game_event,
                                              uint8_t *out_message,
                                              uint16_t capacity,
                                              uint16_t *out_size)
{
    NetEvent event = NET_EVENT__INIT;
    event.event_case = NET_EVENT__EVENT_GAME;
    event.game = game_event;

    NetServerMessage server_message = NET_SERVER_MESSAGE__INIT;
    server_message.data_case = NET_SERVER_MESSAGE__DATA_EVENT;
    server_message.event = &event;

    const size_t packed_size = net_server_message__get_packed_size(&server_message);
    if(packed_size > capacity)
    {
        return RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL;
    }

    *out_size = (uint16_t)net_server_message__pack(&server_message, out_message);
    return RAD_NET_CODEC_OK;
}

RAD_NetCodecResult_t RAD_SerializeCurrentPlayerEventToMessage(RAD_UserId_t user,
                                                                uint8_t *out_message,
                                                                uint16_t capacity,
                                                                uint16_t *out_size)
{
    NetCurrentPlayerEvent current_player = NET_CURRENT_PLAYER_EVENT__INIT;
    current_player.user_id = (uint64_t)user;

    NetGameEvent game_event = NET_GAME_EVENT__INIT;
    game_event.event_case = NET_GAME_EVENT__EVENT_CURRENT_PLAYER;
    game_event.current_player = &current_player;

    return RAD_PackGameEvent(&game_event, out_message, capacity, out_size);
}

RAD_NetCodecResult_t RAD_SerializePlayersEventToMessage(const RAD_UserId_t *users,
                                                          size_t number_of_users,
                                                          uint8_t *out_message,
                                                          uint16_t capacity,
                                                          uint16_t *out_size)
{
    NetPlayersEvent players = NET_PLAYERS_EVENT__INIT;
    players.n_user_ids = number_of_users;
    // RAD_UserId_t ist uint64_t, die Liste geht also ohne Kopie hinein. Der Cast
    // nimmt nur die Konstanz weg, die die generierte Struktur nicht kennt;
    // gepackt wird nur gelesen -- wie bei "description" unten.
    players.user_ids = (uint64_t *)users;

    NetGameEvent game_event = NET_GAME_EVENT__INIT;
    game_event.event_case = NET_GAME_EVENT__EVENT_PLAYERS;
    game_event.players = &players;

    return RAD_PackGameEvent(&game_event, out_message, capacity, out_size);
}

RAD_NetCodecResult_t RAD_SerializeWorldSizeEventToMessage(uint32_t width,
                                                            uint32_t height,
                                                            uint8_t *out_message,
                                                            uint16_t capacity,
                                                            uint16_t *out_size)
{
    NetWorldSizeEvent world_size = NET_WORLD_SIZE_EVENT__INIT;
    world_size.width = width;
    world_size.height = height;

    NetGameEvent game_event = NET_GAME_EVENT__INIT;
    game_event.event_case = NET_GAME_EVENT__EVENT_WORLD_SIZE;
    game_event.world_size = &world_size;

    return RAD_PackGameEvent(&game_event, out_message, capacity, out_size);
}

RAD_NetCodecResult_t RAD_SerializeTilesEventToMessage(const RAD_Tile_t *tiles,
                                                        size_t number_of_tiles,
                                                        uint8_t *out_message,
                                                        uint16_t capacity,
                                                        uint16_t *out_size)
{
    // Eine Kopie in die generierten Strukturen, dazu ein Zeiger je Feld --
    // repeated NetTile ist in protobuf-c ein Array von Zeigern. Mindestens ein
    // Platz, weil ein Array der Laenge null kein gueltiges C ist; gepackt werden
    // nur "number_of_tiles".
    NetTile net_tiles[(number_of_tiles > 0) ? number_of_tiles : 1];
    NetTile *tile_pointers[(number_of_tiles > 0) ? number_of_tiles : 1];

    for(size_t i = 0; i < number_of_tiles; ++i)
    {
        const RAD_Tile_t *tile = &tiles[i];

        net_tile__init(&net_tiles[i]);
        net_tiles[i].x = (uint32_t)tile->x;
        net_tiles[i].y = (uint32_t)tile->y;
        net_tiles[i].z = (uint32_t)tile->z;
        net_tiles[i].type = RAD_NetTileTypeFromTileType(tile->type);
        net_tiles[i].entity_id = tile->entity;

        tile_pointers[i] = &net_tiles[i];
    }

    NetTilesEvent tiles_event = NET_TILES_EVENT__INIT;
    tiles_event.n_tiles = number_of_tiles;
    tiles_event.tiles = tile_pointers;

    NetGameEvent game_event = NET_GAME_EVENT__INIT;
    game_event.event_case = NET_GAME_EVENT__EVENT_TILES;
    game_event.tiles = &tiles_event;

    return RAD_PackGameEvent(&game_event, out_message, capacity, out_size);
}

///
/// RAD_TileType_t -> NetTileType. Ohne default: kommt im Spiel ein Tile-Typ
/// dazu, meldet -Wswitch hier, dass er auf der Strecke noch keinen Namen hat --
/// dasselbe Muster wie RAD_NetFillTile beim Client in der Gegenrichtung.
///
static NetTileType RAD_NetTileTypeFromTileType(RAD_TileType_t type)
{
    switch(type)
    {
        case RAD_TILE_TYPE_VOID:   return NET_TILE_TYPE__TILE_TYPE_VOID;
        case RAD_TILE_TYPE_GROUND: return NET_TILE_TYPE__TILE_TYPE_GROUND;
        case RAD_TILE_TYPE_WATER:  return NET_TILE_TYPE__TILE_TYPE_WATER;
    }

    // Ein Wert ausserhalb der Aufzaehlung -- auf der Strecke ist das "unbekannt".
    return NET_TILE_TYPE__TILE_TYPE_UNKNOWN;
}

RAD_NetCodecResult_t RAD_SerializeCommandResponseToMessage(const RAD_CommandResponse_t *response,
                                                             bool success,
                                                             const char *description,
                                                             uint8_t *out_message,
                                                             uint16_t capacity,
                                                             uint16_t *out_size)
{
    // Vorerst die einzige abgebildete Art -- siehe die Erklaerung in message.h.
    if(response->command.header.type != RAD_COMMAND_TYPE_MOVE_ENTITY)
    {
        return RAD_NET_CODEC_ERROR_UNSUPPORTED_COMMAND_TYPE;
    }

    const RAD_CommandMoveEntity_t *move = &response->command.command.move_entity;
    const int8_t number_of_steps = move->path.number_of_steps;

    // Dieselbe Grenze wie beim Lesen und beim Client (net_codec.c dort).
    if((number_of_steps < 2) || (number_of_steps > RAD_PATH_MAX_STEPS))
    {
        return RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT;
    }

    // Wie beim Client: nur die tatsaechlich genutzten Felder fahren mit,
    // Protobuf traegt seine eigene Laenge (n_steps).
    NetMoveCommandStep steps[RAD_PATH_MAX_STEPS];
    NetMoveCommandStep *step_pointers[RAD_PATH_MAX_STEPS];
    for(int8_t i = 0; i < number_of_steps; ++i)
    {
        net_move_command_step__init(&steps[i]);
        steps[i].x = (uint32_t)move->path.steps_to[i].x;
        steps[i].y = (uint32_t)move->path.steps_to[i].y;
        step_pointers[i] = &steps[i];
    }

    NetMoveCommand move_command = NET_MOVE_COMMAND__INIT;
    // uint64 -> uint32: siehe die Anmerkung beim Client zum Encodieren
    // (net_codec.h dort).
    move_command.user_id = (uint32_t)response->command.header.user;
    move_command.entity_id = (uint32_t)move->entity;
    move_command.n_steps = (size_t)number_of_steps;
    move_command.steps = step_pointers;

    NetCommandRequest command_request = NET_COMMAND_REQUEST__INIT;
    command_request.id = (uint32_t)response->command.header.sequence;
    command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_MOVE;
    command_request.move = &move_command;

    NetCommandResponse command_response = NET_COMMAND_RESPONSE__INIT;
    command_response.command = &command_request;
    command_response.success = success;
    // protobuf-c erwartet fuer ein string-Feld nie NULL, sondern einen leeren
    // String -- anders als bei den oneof-Zeigern oben. Der Cast nimmt nur die
    // Konstanz weg, die die generierte Struktur nicht kennt; gepackt wird
    // "description" nur gelesen.
    command_response.description = (char *)((description != NULL) ? description : "");

    NetServerMessage server_message = NET_SERVER_MESSAGE__INIT;
    server_message.data_case = NET_SERVER_MESSAGE__DATA_COMMAND_RESPONSE;
    server_message.command_response = &command_response;

    const size_t packed_size = net_server_message__get_packed_size(&server_message);
    if(packed_size > capacity)
    {
        return RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL;
    }

    *out_size = (uint16_t)net_server_message__pack(&server_message, out_message);
    return RAD_NET_CODEC_OK;
}
