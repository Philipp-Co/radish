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
    // Ungekuerzt: der Absender ist die gepackte Kennung (net_codec.h).
    move_command.user_id = (uint64_t)request->user;
    move_command.unit_id = (uint32_t)request->entity;
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

RAD_NetCodecResult_t RAD_NetEncodeDeployRequest(const RAD_NetDeployRequest_t *request,
                                                 uint8_t *buffer,
                                                 size_t buffer_size,
                                                 size_t *out_length)
{
    NetDeployCommand deploy_command = NET_DEPLOY_COMMAND__INIT;
    // Ungekuerzt: der Absender ist die gepackte Kennung (net_codec.h).
    deploy_command.user_id = (uint64_t)request->user;
    deploy_command.unit_id = (uint32_t)request->entity;
    deploy_command.x = (uint32_t)request->position.x;
    deploy_command.y = (uint32_t)request->position.y;

    NetCommandRequest command_request = NET_COMMAND_REQUEST__INIT;
    command_request.id = (uint32_t)request->sequence;
    command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_DEPLOY;
    command_request.deploy = &deploy_command;

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

RAD_NetCodecResult_t RAD_NetEncodeAttackRequest(const RAD_NetAttackRequest_t *request,
                                                 uint8_t *buffer,
                                                 size_t buffer_size,
                                                 size_t *out_length)
{
    NetAttackCommand attack_command = NET_ATTACK_COMMAND__INIT;
    // Ungekuerzt: der Absender ist die gepackte Kennung (net_codec.h).
    attack_command.user_id = (uint64_t)request->user;
    attack_command.unit_id = (uint32_t)request->entity;
    attack_command.x = (uint32_t)request->target.x;
    attack_command.y = (uint32_t)request->target.y;

    NetCommandRequest command_request = NET_COMMAND_REQUEST__INIT;
    command_request.id = (uint32_t)request->sequence;
    command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_ATTACK;
    command_request.attack = &attack_command;

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

RAD_NetCodecResult_t RAD_NetEncodeEndTurnRequest(const RAD_NetEndTurnRequest_t *request,
                                                  uint8_t *buffer,
                                                  size_t buffer_size,
                                                  size_t *out_length)
{
    NetEndTurnCommand end_turn_command = NET_END_TURN_COMMAND__INIT;
    // Ungekuerzt: der Absender ist die gepackte Kennung (net_codec.h).
    end_turn_command.user_id = (uint64_t)request->user;

    NetCommandRequest command_request = NET_COMMAND_REQUEST__INIT;
    command_request.id = (uint32_t)request->sequence;
    command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_END_TURN;
    command_request.end_turn = &end_turn_command;

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

RAD_NetCodecResult_t RAD_NetEncodeDiscoverReserve(uint8_t *buffer,
                                                   size_t buffer_size,
                                                   size_t *out_length)
{
    // "reserved" bleibt 0 und wird damit gar nicht geschrieben (discover.proto).
    NetDiscoverReserveRequest reserve = NET_DISCOVER_RESERVE_REQUEST__INIT;

    NetUserRequest user_request = NET_USER_REQUEST__INIT;
    user_request.data_case = NET_USER_REQUEST__DATA_DISCOVER_RESERVE_REQUEST;
    user_request.discover_reserve_request = &reserve;

    const size_t packed_size = net_user_request__get_packed_size(&user_request);
    if(packed_size > buffer_size)
    {
        return RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL;
    }

    *out_length = net_user_request__pack(&user_request, buffer);
    return RAD_NET_CODEC_OK;
}

RAD_NetCodecResult_t RAD_NetEncodeDiscoverUnits(uint8_t *buffer,
                                                 size_t buffer_size,
                                                 size_t *out_length)
{
    // "reserved" bleibt 0 und wird damit gar nicht geschrieben (discover.proto).
    NetDiscoverUnitsRequest units = NET_DISCOVER_UNITS_REQUEST__INIT;

    NetUserRequest user_request = NET_USER_REQUEST__INIT;
    user_request.data_case = NET_USER_REQUEST__DATA_DISCOVER_UNITS_REQUEST;
    user_request.discover_units_request = &units;

    const size_t packed_size = net_user_request__get_packed_size(&user_request);
    if(packed_size > buffer_size)
    {
        return RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL;
    }

    *out_length = net_user_request__pack(&user_request, buffer);
    return RAD_NET_CODEC_OK;
}

static void RAD_NetFillTile(RAD_NetTile_t *out, const NetTile *tile);
static RAD_NetCodecResult_t RAD_NetFillUnit(RAD_NetUnit_t *out, const NetUnit *unit);
static RAD_NetCodecResult_t RAD_NetDispatchTiles(RAD_NetEventManager_t *events, const NetTilesEvent *tiles_event);

///
/// Die Antwort auf ein Deployment: wie die auf einen Zug, nur mit Einheit und
/// Feld statt eines Weges.
///
static RAD_NetCodecResult_t RAD_NetDispatchDeployResponse(RAD_NetEventManager_t *events,
                                                           const NetCommandResponse *command_response,
                                                           const NetDeployCommand *deploy)
{
    RAD_NetDeployResponse_t response;
    memset(&response, 0, sizeof(response));

    // Sequenznummer und Absender wie beim Zug (RAD_NetDispatchCommandResponse).
    response.sequence = (RAD_NetSequence_t)command_response->command->id;
    response.user = (RAD_NetUserId_t)deploy->user_id;
    response.success = command_response->success;
    response.entity = (RAD_NetEntityId_t)deploy->unit_id;
    response.position.x = (int16_t)deploy->x;
    response.position.y = (int16_t)deploy->y;

    RAD_NetEventManagerPublishDeployResponse(events, &response);
    return RAD_NET_CODEC_OK;
}

///
/// Die Antwort auf einen Angriff: wie die auf ein Deployment, dazu der Grund,
/// falls der Server ihn abgelehnt hat.
///
static RAD_NetCodecResult_t RAD_NetDispatchAttackResponse(RAD_NetEventManager_t *events,
                                                           const NetCommandResponse *command_response,
                                                           const NetAttackCommand *attack)
{
    RAD_NetAttackResponse_t response;
    memset(&response, 0, sizeof(response));

    response.sequence = (RAD_NetSequence_t)command_response->command->id;
    response.user = (RAD_NetUserId_t)attack->user_id;
    response.success = command_response->success;
    response.entity = (RAD_NetEntityId_t)attack->unit_id;
    response.target.x = (int16_t)attack->x;
    response.target.y = (int16_t)attack->y;
    if(command_response->description != NULL)
    {
        // Gekuerzt, wenn sie zu lang ist; memset oben sorgt fuer die Null.
        strncpy(response.description, command_response->description, sizeof(response.description) - 1);
    }

    RAD_NetEventManagerPublishAttackResponse(events, &response);
    return RAD_NET_CODEC_OK;
}

///
/// Die Antwort auf ein Abgeben: nur Kopf und Erfolg.
///
static RAD_NetCodecResult_t RAD_NetDispatchEndTurnResponse(RAD_NetEventManager_t *events,
                                                            const NetCommandResponse *command_response,
                                                            const NetEndTurnCommand *end_turn)
{
    RAD_NetEndTurnResponse_t response;
    memset(&response, 0, sizeof(response));

    // Sequenznummer und Absender wie beim Zug (RAD_NetDispatchCommandResponse).
    response.sequence = (RAD_NetSequence_t)command_response->command->id;
    response.user = (RAD_NetUserId_t)end_turn->user_id;
    response.success = command_response->success;

    RAD_NetEventManagerPublishEndTurnResponse(events, &response);
    return RAD_NET_CODEC_OK;
}

static RAD_NetCodecResult_t RAD_NetDispatchCommandResponse(RAD_NetEventManager_t *events, const NetCommandResponse *command_response)
{
    // Ohne eingebettetes NetCommandRequest ist das strukturell keine Antwort
    // auf ein Kommando.
    if((command_response == NULL) || (command_response->command == NULL))
    {
        return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }

    const NetCommandRequest *request = command_response->command;

    // Zug, Deployment, Abgeben und Angriff sind im Client abgebildet -- eine
    // Antwort ganz ohne Zweig (commands_case NOT_SET) nicht (net_codec.h).
    if((request->commands_case == NET_COMMAND_REQUEST__COMMANDS_DEPLOY) && (request->deploy != NULL))
    {
        return RAD_NetDispatchDeployResponse(events, command_response, request->deploy);
    }
    if((request->commands_case == NET_COMMAND_REQUEST__COMMANDS_END_TURN) && (request->end_turn != NULL))
    {
        return RAD_NetDispatchEndTurnResponse(events, command_response, request->end_turn);
    }
    if((request->commands_case == NET_COMMAND_REQUEST__COMMANDS_ATTACK) && (request->attack != NULL))
    {
        return RAD_NetDispatchAttackResponse(events, command_response, request->attack);
    }
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

    // Die Sequenznummer uint32 -> uint64: erweitert, nicht abgeschnitten -- die
    // Umkehrung der Anmerkung beim Encodieren. Der Absender ist auf beiden Seiten
    // uint64.
    response.sequence = (RAD_NetSequence_t)request->id;
    response.user = (RAD_NetUserId_t)move->user_id;
    response.success = command_response->success;

    response.entity = (RAD_NetEntityId_t)move->unit_id;
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

        case NET_GAME_EVENT__EVENT_RESERVE_UNIT:
        {
            if((game_event->reserve_unit == NULL) || (game_event->reserve_unit->unit == NULL))
            {
                return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            }

            RAD_NetReserveUnit_t reserve_unit;
            const RAD_NetCodecResult_t result = RAD_NetFillUnit(&reserve_unit, game_event->reserve_unit->unit);
            if(result != RAD_NET_CODEC_OK)
            {
                return result;
            }

            RAD_NetEventManagerPublishReserveUnit(events, &reserve_unit);
            return RAD_NET_CODEC_OK;
        }

        case NET_GAME_EVENT__EVENT_UNIT_DEPLOYED:
        {
            const NetUnitDeployedEvent *unit_deployed = game_event->unit_deployed;
            if((unit_deployed == NULL) || (unit_deployed->unit == NULL))
            {
                return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            }

            // Ein Feld, das in keine Position (int16) passt, gibt es in keiner
            // Welt, die der Client halten kann -- der Server lehnt es beim
            // Aufstellen genauso ab.
            if((unit_deployed->x > INT16_MAX) || (unit_deployed->y > INT16_MAX))
            {
                return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            }

            RAD_NetUnitDeployed_t deployed;
            const RAD_NetCodecResult_t result = RAD_NetFillUnit(&deployed.unit, unit_deployed->unit);
            if(result != RAD_NET_CODEC_OK)
            {
                return result;
            }
            deployed.position.x = (int16_t)unit_deployed->x;
            deployed.position.y = (int16_t)unit_deployed->y;

            RAD_NetEventManagerPublishUnitDeployed(events, &deployed);
            return RAD_NET_CODEC_OK;
        }

        case NET_GAME_EVENT__EVENT_UNIT:
        {
            if((game_event->unit == NULL) || (game_event->unit->unit == NULL))
            {
                return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            }

            RAD_NetUnit_t net_unit;
            const RAD_NetCodecResult_t result = RAD_NetFillUnit(&net_unit, game_event->unit->unit);
            if(result != RAD_NET_CODEC_OK)
            {
                return result;
            }

            RAD_NetEventManagerPublishUnit(events, &net_unit);
            return RAD_NET_CODEC_OK;
        }

        case NET_GAME_EVENT__EVENT__NOT_SET:
        default:
            return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }
}

///
/// Kopiert einen Namen aus der Nachricht nach "out" (RAD_NET_UNIT_NAME_MAX).
/// Gekuerzt, wenn er laenger ist: angezeigt wird er nur (net_types.h).
/// protobuf-c laesst ein leeres string-Feld nie NULL, sondern "" -- abgefangen
/// wird es trotzdem.
///
static void RAD_NetCopyName(char *out, const char *name)
{
    memset(out, 0, RAD_NET_UNIT_NAME_MAX);
    if(name != NULL)
    {
        strncpy(out, name, RAD_NET_UNIT_NAME_MAX - 1);
    }
}

///
/// NetUnit (game.proto) -> RAD_NetUnit_t (io/net_types.h), vollstaendig, fuer
/// alle drei Ereignisse, die eine Einheit tragen.
///
/// Mehr Mitglieder oder Waffen, als die Einheit im Spiel haben kann
/// (RAD_NET_UNIT_MEMBERS_MAX, RAD_NET_MEMBER_WEAPONS_MAX), schickt kein Server,
/// der dieses Protokoll spricht: RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE, und
/// "out" ist unbrauchbar.
///
static RAD_NetCodecResult_t RAD_NetFillUnit(RAD_NetUnit_t *out, const NetUnit *unit)
{
    memset(out, 0, sizeof(*out));

    if(unit->n_members > RAD_NET_UNIT_MEMBERS_MAX)
    {
        return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }

    out->unit = (RAD_NetEntityId_t)unit->unit_id;
    out->owner = (RAD_NetUserId_t)unit->owner_id;
    RAD_NetCopyName(out->name, unit->name);
    out->movement = unit->movement;
    out->transport_capacity = unit->transport_capacity;
    out->can_capture = unit->can_capture;
    out->deployed = unit->deployed;
    out->moved = unit->moved;
    out->attacked = unit->attacked;

    for(size_t m = 0; m < unit->n_members; ++m)
    {
        const NetUnitMember *member = unit->members[m];
        if((member == NULL) || (member->n_weapons > RAD_NET_MEMBER_WEAPONS_MAX))
        {
            return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
        }

        RAD_NetUnitMember_t *net_member = &out->members[m];
        RAD_NetCopyName(net_member->profile, member->profile);
        net_member->health = member->health;
        net_member->armor = member->armor;
        net_member->strength = member->strength;
        net_member->accuracy = member->accuracy;

        for(size_t w = 0; w < member->n_weapons; ++w)
        {
            const NetWeapon *weapon = member->weapons[w];
            if(weapon == NULL)
            {
                return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
            }

            RAD_NetWeapon_t *net_weapon = &net_member->weapons[w];
            RAD_NetCopyName(net_weapon->name, weapon->name);
            net_weapon->weapon_class = weapon->weapon_class;
            net_weapon->shots = weapon->shots;
            net_weapon->strength = weapon->strength;
            net_weapon->min_range = weapon->min_range;
            net_weapon->max_range = weapon->max_range;
            net_weapon->penetration = weapon->penetration;
        }
        net_member->number_of_weapons = (uint32_t)member->n_weapons;
    }
    out->number_of_members = (uint32_t)unit->n_members;

    return RAD_NET_CODEC_OK;
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
    out->entity_id = (RAD_NetEntityId_t)tile->unit_id;

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
