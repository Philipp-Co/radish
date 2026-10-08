#include <radish/server/interface/message.h>

#include <stdint.h>
#include <string.h>

// Generierter Code aus protobuf/*.proto (protobuf/CMakeLists.txt, Ziel
// radish_protobuf). Bleibt auf diese Datei beschraenkt -- message.h zeigt nach
// aussen nur RAD_Command_t/RAD_CommandResponse_t und die eigene
// RAD_NetCodecResult_t, keinen generierten NetXxx-Typ und kein protobuf-c-
// Symbol.
#include "message.pb-c.h"

static RAD_NetCodecResult_t RAD_ParseMoveCommand(const NetCommandRequest *command_request, RAD_Command_t *out_command);
static RAD_NetCodecResult_t RAD_ParseDeployCommand(const NetCommandRequest *command_request, RAD_Command_t *out_command);
static RAD_NetCodecResult_t RAD_ParseEndTurnCommand(const NetCommandRequest *command_request, RAD_Command_t *out_command);
static RAD_NetCodecResult_t RAD_ParseAttackCommand(const NetCommandRequest *command_request, RAD_Command_t *out_command);


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
        case RAD_NET_CODEC_ERROR_NO_SENDER:                 return "Nachricht ohne Absender";
        default:                                            return "unbekanntes Ergebnis";
    }
}

/// Laenge des Absenders vor der Nutzlast (message.h).
#define RAD_SENDER_SIZE 8

RAD_NetCodecResult_t RAD_ParseSenderFromMessage(const uint8_t *message, uint16_t size,
                                                 RAD_UserId_t *sender,
                                                 const uint8_t **payload, uint16_t *payload_size)
{
    if((message == NULL) || (size < RAD_SENDER_SIZE))
    {
        return RAD_NET_CODEC_ERROR_NO_SENDER;
    }

    // Big-Endian, wie der Zucchini-Code davor: das hoechste Byte zuerst.
    RAD_UserId_t user = 0;
    for(int i = 0; i < RAD_SENDER_SIZE; ++i)
    {
        user = (user << 8) | (RAD_UserId_t)message[i];
    }

    *sender = user;
    *payload = message + RAD_SENDER_SIZE;
    *payload_size = (uint16_t)(size - RAD_SENDER_SIZE);
    return RAD_NET_CODEC_OK;
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

    // Abgebildet sind move, deploy, end_turn und attack -- eine Nachricht ganz
    // ohne Zweig (commands_case NOT_SET) laesst sich in kein RAD_Command_t
    // uebersetzen (message.h). "out_command" bleibt dann unberuehrt.
    RAD_Command_t parsed;
    memset(&parsed, 0, sizeof(parsed));

    RAD_NetCodecResult_t result = RAD_NET_CODEC_ERROR_UNSUPPORTED_COMMAND_TYPE;
    if((command_request->commands_case == NET_COMMAND_REQUEST__COMMANDS_MOVE) && (command_request->move != NULL))
    {
        result = RAD_ParseMoveCommand(command_request, &parsed);
    }
    else if((command_request->commands_case == NET_COMMAND_REQUEST__COMMANDS_DEPLOY) && (command_request->deploy != NULL))
    {
        result = RAD_ParseDeployCommand(command_request, &parsed);
    }
    else if((command_request->commands_case == NET_COMMAND_REQUEST__COMMANDS_END_TURN) && (command_request->end_turn != NULL))
    {
        result = RAD_ParseEndTurnCommand(command_request, &parsed);
    }
    else if((command_request->commands_case == NET_COMMAND_REQUEST__COMMANDS_ATTACK) && (command_request->attack != NULL))
    {
        result = RAD_ParseAttackCommand(command_request, &parsed);
    }

    if(result == RAD_NET_CODEC_OK)
    {
        *out_command = parsed;
    }

    net_user_request__free_unpacked(request, NULL);
    return result;
}

///
/// Der move-Zweig. Die Sequenznummer uint32 -> uint64: erweitert, nicht
/// abgeschnitten -- die Umkehrung der Anmerkung beim Client zum Encodieren
/// (net_codec.h dort). Der Absender ist auf beiden Seiten uint64.
///
static RAD_NetCodecResult_t RAD_ParseMoveCommand(const NetCommandRequest *command_request, RAD_Command_t *out_command)
{
    const NetMoveCommand *move = command_request->move;
    const size_t number_of_steps = move->n_steps;

    // Dieselbe Grenze wie beim Client (net_codec.c dort) und beim alten Codec.
    if((number_of_steps < 2) || (number_of_steps > RAD_PATH_MAX_STEPS))
    {
        return RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT;
    }

    out_command->header.type = RAD_COMMAND_TYPE_MOVE_UNIT;
    out_command->header.sequence = (RAD_CommandSequence_t)command_request->id;
    out_command->header.user = (RAD_UserId_t)move->user_id;

    out_command->command.move_unit.unit = (RAD_UnitId_t)move->unit_id;
    out_command->command.move_unit.path.number_of_steps = (int8_t)number_of_steps;
    for(size_t i = 0; i < number_of_steps; ++i)
    {
        out_command->command.move_unit.path.steps_to[i].x = (int16_t)move->steps[i]->x;
        out_command->command.move_unit.path.steps_to[i].y = (int16_t)move->steps[i]->y;
    }
    // Die ungenutzten Plaetze bleiben genullt (memset beim Aufrufer) -- wie beim
    // alten Codec, dasselbe Kommando ergibt so immer dieselbe Struktur.

    return RAD_NET_CODEC_OK;
}

///
/// Der deploy-Zweig. Ein Feld, das nicht in int16 passt, ist keines dieser Welt
/// und wuerde beim Umwandeln ein anderes -- die Nachricht ist dann nicht, was ihr
/// Zweig behauptet.
///
static RAD_NetCodecResult_t RAD_ParseDeployCommand(const NetCommandRequest *command_request, RAD_Command_t *out_command)
{
    const NetDeployCommand *deploy = command_request->deploy;

    if((deploy->x > (uint32_t)INT16_MAX) || (deploy->y > (uint32_t)INT16_MAX))
    {
        return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }

    out_command->header.type = RAD_COMMAND_TYPE_DEPLOY_UNIT;
    out_command->header.sequence = (RAD_CommandSequence_t)command_request->id;
    out_command->header.user = (RAD_UserId_t)deploy->user_id;

    out_command->command.deploy_unit.unit = (RAD_UnitId_t)deploy->unit_id;
    out_command->command.deploy_unit.x = (int16_t)deploy->x;
    out_command->command.deploy_unit.y = (int16_t)deploy->y;

    return RAD_NET_CODEC_OK;
}

///
/// Der end_turn-Zweig: nur der Kopf, eine Nutzlast hat das Kommando nicht
/// (RAD_COMMAND_TYPE_END_TURN, command.h).
///
static RAD_NetCodecResult_t RAD_ParseEndTurnCommand(const NetCommandRequest *command_request, RAD_Command_t *out_command)
{
    out_command->header.type = RAD_COMMAND_TYPE_END_TURN;
    out_command->header.sequence = (RAD_CommandSequence_t)command_request->id;
    out_command->header.user = (RAD_UserId_t)command_request->end_turn->user_id;

    return RAD_NET_CODEC_OK;
}

///
/// Der attack-Zweig. Das Zielfeld wie beim deploy-Zweig: was nicht in int16
/// passt, ist keines dieser Welt.
///
static RAD_NetCodecResult_t RAD_ParseAttackCommand(const NetCommandRequest *command_request, RAD_Command_t *out_command)
{
    const NetAttackCommand *attack = command_request->attack;

    if((attack->x > (uint32_t)INT16_MAX) || (attack->y > (uint32_t)INT16_MAX))
    {
        return RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
    }

    out_command->header.type = RAD_COMMAND_TYPE_ATTACK;
    out_command->header.sequence = (RAD_CommandSequence_t)command_request->id;
    out_command->header.user = (RAD_UserId_t)attack->user_id;

    out_command->command.attack.unit = (RAD_UnitId_t)attack->unit_id;
    out_command->command.attack.x = (int16_t)attack->x;
    out_command->command.attack.y = (int16_t)attack->y;

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

RAD_NetCodecResult_t RAD_ParseDiscoverReserveFromMessage(const uint8_t *message, uint16_t size)
{
    NetUserRequest *request = net_user_request__unpack(NULL, size, message);
    if(request == NULL)
    {
        return RAD_NET_CODEC_ERROR_DECODE_FAILED;
    }

    const bool reserve = (request->data_case == NET_USER_REQUEST__DATA_DISCOVER_RESERVE_REQUEST)
                         && (request->discover_reserve_request != NULL);

    net_user_request__free_unpacked(request, NULL);
    return reserve ? RAD_NET_CODEC_OK : RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
}

RAD_NetCodecResult_t RAD_ParseDiscoverUnitsFromMessage(const uint8_t *message, uint16_t size)
{
    NetUserRequest *request = net_user_request__unpack(NULL, size, message);
    if(request == NULL)
    {
        return RAD_NET_CODEC_ERROR_DECODE_FAILED;
    }

    const bool units = (request->data_case == NET_USER_REQUEST__DATA_DISCOVER_UNITS_REQUEST)
                       && (request->discover_units_request != NULL);

    net_user_request__free_unpacked(request, NULL);
    return units ? RAD_NET_CODEC_OK : RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE;
}

///
/// Eine NetUnit samt allem, worauf sie zeigt. protobuf-c packt ueber Zeiger: die
/// Mitglieder und Waffen muessen leben, bis gepackt ist, und stehen deshalb hier
/// und nicht in RAD_NetUnitFromUnit. Der Aufrufer legt sie auf seinen Stapel.
///
typedef struct
{
    NetUnit unit;
    NetUnitMember members[RAD_UNIT_MAX_MEMBERS];
    NetUnitMember *member_pointers[RAD_UNIT_MAX_MEMBERS];
    NetWeapon weapons[RAD_UNIT_MAX_MEMBERS][RAD_UNIT_MAX_WEAPONS];
    NetWeapon *weapon_pointers[RAD_UNIT_MAX_MEMBERS][RAD_UNIT_MAX_WEAPONS];
} RAD_NetUnitStorage_t;

/// Eine Anzahl aus dem Spiel, auf [0, maximum] begrenzt -- mehr Platz ist in
/// RAD_NetUnitStorage_t nicht.
static size_t RAD_NetClampCount(int32_t count, int32_t maximum)
{
    return (size_t)((count < 0) ? 0 : (count > maximum) ? maximum : count);
}

/// Ein Wert aus dem Spiel fuer ein uint32-Feld. Negativ ist keiner; kaeme doch
/// einer, wird er 0 statt riesig.
static uint32_t RAD_NetUnsigned(int16_t value)
{
    return (value < 0) ? 0u : (uint32_t)value;
}

///
/// RAD_Unit_t -> NetUnit, vollstaendig, fuer die Reserve, die Einheiten und das
/// Aufstellen. "storage->unit" ist danach die NetUnit; sie zeigt auf die Namen in
/// "unit" und auf den Rest von "storage" und gilt nur, solange beide leben.
///
static void RAD_NetUnitFromUnit(const RAD_Unit_t *unit, RAD_NetUnitStorage_t *storage)
{
    NetUnit *net_unit = &storage->unit;
    net_unit__init(net_unit);
    net_unit->unit_id = (uint32_t)unit->id;
    net_unit->owner_id = (uint64_t)unit->owner;
    // Wie bei "description" in der Antwort: protobuf-c will einen String und nimmt
    // ihn nur zum Lesen; der Cast nimmt die Konstanz weg, die es nicht kennt.
    net_unit->name = (char *)unit->name;
    net_unit->number_of_members = (uint32_t)unit->number_of_members;
    net_unit->movement = RAD_NetUnsigned(unit->movement);
    net_unit->transport_capacity = RAD_NetUnsigned(unit->transport_capacity);
    net_unit->can_capture = unit->can_capture;
    net_unit->deployed = unit->turn.deployed;
    net_unit->moved = unit->turn.moved;
    net_unit->attacked = unit->turn.attacked;

    const size_t number_of_members = RAD_NetClampCount(unit->number_of_members, RAD_UNIT_MAX_MEMBERS);
    for(size_t m = 0; m < number_of_members; ++m)
    {
        const RAD_UnitMember_t *member = &unit->members[m];
        NetUnitMember *net_member = &storage->members[m];
        net_unit_member__init(net_member);
        net_member->profile = (char *)member->profile;
        net_member->health = RAD_NetUnsigned(member->health);
        net_member->armor = RAD_NetUnsigned(member->armor);
        net_member->strength = RAD_NetUnsigned(member->strength);
        net_member->accuracy = RAD_NetUnsigned(member->accuracy);

        const size_t number_of_weapons = RAD_NetClampCount(member->number_of_weapons, RAD_UNIT_MAX_WEAPONS);
        for(size_t w = 0; w < number_of_weapons; ++w)
        {
            const RAD_Weapon_t *weapon = &member->weapons[w];
            NetWeapon *net_weapon = &storage->weapons[m][w];
            net_weapon__init(net_weapon);
            net_weapon->name = (char *)weapon->name;
            net_weapon->weapon_class = (uint32_t)weapon->weapon_class;
            net_weapon->shots = RAD_NetUnsigned(weapon->shots);
            net_weapon->strength = RAD_NetUnsigned(weapon->strength);
            net_weapon->min_range = RAD_NetUnsigned(weapon->min_range);
            net_weapon->max_range = RAD_NetUnsigned(weapon->max_range);
            net_weapon->penetration = RAD_NetUnsigned(weapon->penetration);
            storage->weapon_pointers[m][w] = net_weapon;
        }
        net_member->n_weapons = number_of_weapons;
        net_member->weapons = storage->weapon_pointers[m];

        storage->member_pointers[m] = net_member;
    }
    net_unit->n_members = number_of_members;
    net_unit->members = storage->member_pointers;
}

RAD_NetCodecResult_t RAD_SerializeUnitEventToMessage(const RAD_Unit_t *unit,
                                                      uint8_t *out_message,
                                                      uint16_t capacity,
                                                      uint16_t *out_size)
{
    RAD_NetUnitStorage_t storage;
    RAD_NetUnitFromUnit(unit, &storage);

    NetUnitEvent unit_event = NET_UNIT_EVENT__INIT;
    unit_event.unit = &storage.unit;

    NetGameEvent game_event = NET_GAME_EVENT__INIT;
    game_event.event_case = NET_GAME_EVENT__EVENT_UNIT;
    game_event.unit = &unit_event;

    return RAD_PackGameEvent(&game_event, out_message, capacity, out_size);
}

RAD_NetCodecResult_t RAD_SerializeUnitDeployedEventToMessage(const RAD_Unit_t *unit,
                                                               int32_t x,
                                                               int32_t y,
                                                               uint8_t *out_message,
                                                               uint16_t capacity,
                                                               uint16_t *out_size)
{
    RAD_NetUnitStorage_t storage;
    RAD_NetUnitFromUnit(unit, &storage);

    NetUnitDeployedEvent unit_deployed = NET_UNIT_DEPLOYED_EVENT__INIT;
    unit_deployed.unit = &storage.unit;
    unit_deployed.x = (uint32_t)x;
    unit_deployed.y = (uint32_t)y;

    NetGameEvent game_event = NET_GAME_EVENT__INIT;
    game_event.event_case = NET_GAME_EVENT__EVENT_UNIT_DEPLOYED;
    game_event.unit_deployed = &unit_deployed;

    return RAD_PackGameEvent(&game_event, out_message, capacity, out_size);
}

RAD_NetCodecResult_t RAD_SerializeReserveUnitEventToMessage(const RAD_Unit_t *unit,
                                                              uint8_t *out_message,
                                                              uint16_t capacity,
                                                              uint16_t *out_size)
{
    RAD_NetUnitStorage_t storage;
    RAD_NetUnitFromUnit(unit, &storage);

    NetReserveUnitEvent reserve_unit = NET_RESERVE_UNIT_EVENT__INIT;
    reserve_unit.unit = &storage.unit;

    NetGameEvent game_event = NET_GAME_EVENT__INIT;
    game_event.event_case = NET_GAME_EVENT__EVENT_RESERVE_UNIT;
    game_event.reserve_unit = &reserve_unit;

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
        net_tiles[i].unit_id = tile->unit;

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
    NetCommandRequest command_request = NET_COMMAND_REQUEST__INIT;
    command_request.id = (uint32_t)response->command.header.sequence;

    // Die Zweige stehen hier und nicht in eigenen Funktionen: protobuf-c packt ueber
    // Zeiger, und alles, worauf sie zeigen, muss bis zum Packen unten leben.
    NetMoveCommandStep steps[RAD_PATH_MAX_STEPS];
    NetMoveCommandStep *step_pointers[RAD_PATH_MAX_STEPS];
    NetMoveCommand move_command = NET_MOVE_COMMAND__INIT;
    NetDeployCommand deploy_command = NET_DEPLOY_COMMAND__INIT;
    NetEndTurnCommand end_turn_command = NET_END_TURN_COMMAND__INIT;
    NetAttackCommand attack_command = NET_ATTACK_COMMAND__INIT;

    switch(response->command.header.type)
    {
        case RAD_COMMAND_TYPE_MOVE_UNIT:
        {
            const RAD_CommandMoveUnit_t *move = &response->command.command.move_unit;
            const int8_t number_of_steps = move->path.number_of_steps;

            // Dieselbe Grenze wie beim Lesen und beim Client (net_codec.c dort).
            if((number_of_steps < 2) || (number_of_steps > RAD_PATH_MAX_STEPS))
            {
                return RAD_NET_CODEC_ERROR_INVALID_STEP_COUNT;
            }

            // Wie beim Client: nur die tatsaechlich genutzten Felder fahren mit,
            // Protobuf traegt seine eigene Laenge (n_steps).
            for(int8_t i = 0; i < number_of_steps; ++i)
            {
                net_move_command_step__init(&steps[i]);
                steps[i].x = (uint32_t)move->path.steps_to[i].x;
                steps[i].y = (uint32_t)move->path.steps_to[i].y;
                step_pointers[i] = &steps[i];
            }

            // Ungekuerzt: der Absender ist die gepackte Kennung (start_game.h).
            move_command.user_id = (uint64_t)response->command.header.user;
            move_command.unit_id = (uint32_t)move->unit;
            move_command.n_steps = (size_t)number_of_steps;
            move_command.steps = step_pointers;

            command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_MOVE;
            command_request.move = &move_command;
            break;
        }

        case RAD_COMMAND_TYPE_DEPLOY_UNIT:
        {
            const RAD_CommandDeployUnit_t *deploy = &response->command.command.deploy_unit;

            deploy_command.user_id = (uint64_t)response->command.header.user;
            deploy_command.unit_id = (uint32_t)deploy->unit;
            deploy_command.x = (uint32_t)deploy->x;
            deploy_command.y = (uint32_t)deploy->y;

            command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_DEPLOY;
            command_request.deploy = &deploy_command;
            break;
        }

        case RAD_COMMAND_TYPE_END_TURN:
        {
            end_turn_command.user_id = (uint64_t)response->command.header.user;

            command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_END_TURN;
            command_request.end_turn = &end_turn_command;
            break;
        }

        case RAD_COMMAND_TYPE_ATTACK:
        {
            const RAD_CommandAttack_t *attack = &response->command.command.attack;

            attack_command.user_id = (uint64_t)response->command.header.user;
            attack_command.unit_id = (uint32_t)attack->unit;
            attack_command.x = (uint32_t)attack->x;
            attack_command.y = (uint32_t)attack->y;

            command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_ATTACK;
            command_request.attack = &attack_command;
            break;
        }

        // Die uebrigen Arten sind in command.proto nicht abgebildet (message.h).
        default:
            return RAD_NET_CODEC_ERROR_UNSUPPORTED_COMMAND_TYPE;
    }

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
