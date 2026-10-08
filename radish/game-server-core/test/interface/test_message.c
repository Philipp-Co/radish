#include <unity.h>
#include <stdint.h>
#include <string.h>

#include <radish/server/interface/message.h>

// Der Test baut die Nachrichten selbst, mit dem generierten Code -- so, wie ein
// Client sie schickt. Die Datei selbst haelt diese Typen nach aussen verborgen
// (message.h); ein Test sieht sie, weil er die Gegenseite spielt.
#include "message.pb-c.h"

///
/// Das Aufstellen auf der Strecke (NetDeployCommand, protobuf/command.proto): aus
/// einer Nachricht wird ein RAD_CommandDeployUnit_t, und die Antwort traegt es
/// zurueck.
///

/// Packt ein Deploy, wie ein Client es schickt, nach "buffer"; liefert die Laenge.
static uint16_t packe_deploy(uint8_t *buffer, size_t capacity, uint64_t user, uint32_t unit, uint32_t x, uint32_t y)
{
    NetDeployCommand deploy = NET_DEPLOY_COMMAND__INIT;
    deploy.user_id = user;
    deploy.unit_id = unit;
    deploy.x = x;
    deploy.y = y;

    NetCommandRequest command_request = NET_COMMAND_REQUEST__INIT;
    command_request.id = 7;
    command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_DEPLOY;
    command_request.deploy = &deploy;

    NetUserRequest request = NET_USER_REQUEST__INIT;
    request.data_case = NET_USER_REQUEST__DATA_COMMAND_REQUEST;
    request.command_request = &command_request;

    const size_t size = net_user_request__get_packed_size(&request);
    TEST_ASSERT_TRUE(size <= capacity);
    return (uint16_t)net_user_request__pack(&request, buffer);
}

void test_message_liest_ein_deploy_kommando(void)
{
    uint8_t buffer[256];
    // Die Id braucht alle 64 Bit -- die gepackte Kennung "aB3xK9pQ".
    const uint16_t size = packe_deploy(buffer, sizeof(buffer), 0x614233784B397051ull, 3, 4, 5);

    RAD_Command_t command;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK, RAD_ParseCommandFromMessage(buffer, size, &command));

    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_TYPE_DEPLOY_UNIT, command.header.type);
    TEST_ASSERT_EQUAL_UINT64(7, command.header.sequence);
    TEST_ASSERT_EQUAL_UINT64(0x614233784B397051ull, command.header.user);
    TEST_ASSERT_EQUAL_INT(3, command.command.deploy_unit.unit);
    TEST_ASSERT_EQUAL_INT(4, command.command.deploy_unit.x);
    TEST_ASSERT_EQUAL_INT(5, command.command.deploy_unit.y);
}

void test_message_lehnt_ein_feld_jenseits_von_int16_ab(void)
{
    uint8_t buffer[256];
    const uint16_t size = packe_deploy(buffer, sizeof(buffer), 1, 3, 40000, 0);

    RAD_Command_t command;
    memset(&command, 0x5A, sizeof(command));
    RAD_Command_t vorher = command;

    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE, RAD_ParseCommandFromMessage(buffer, size, &command));
    TEST_ASSERT_EQUAL_MEMORY(&vorher, &command, sizeof(command));
}

void test_message_packt_die_antwort_auf_ein_deploy(void)
{
    RAD_CommandResponse_t response;
    memset(&response, 0, sizeof(response));
    response.command.header.type = RAD_COMMAND_TYPE_DEPLOY_UNIT;
    response.command.header.sequence = 9;
    response.command.header.user = 0x614233784B397051ull;
    response.command.command.deploy_unit.unit = 2;
    response.command.command.deploy_unit.x = 6;
    response.command.command.deploy_unit.y = 1;
    response.header = response.command.header;

    uint8_t buffer[256];
    uint16_t size = 0;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK,
        RAD_SerializeCommandResponseToMessage(&response, false, "Feld ist besetzt", buffer, sizeof(buffer), &size));

    NetServerMessage *message = net_server_message__unpack(NULL, size, buffer);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_EQUAL_INT(NET_SERVER_MESSAGE__DATA_COMMAND_RESPONSE, message->data_case);

    const NetCommandResponse *command_response = message->command_response;
    TEST_ASSERT_FALSE(command_response->success);
    TEST_ASSERT_EQUAL_STRING("Feld ist besetzt", command_response->description);

    const NetCommandRequest *command_request = command_response->command;
    TEST_ASSERT_EQUAL_UINT32(9, command_request->id);
    TEST_ASSERT_EQUAL_INT(NET_COMMAND_REQUEST__COMMANDS_DEPLOY, command_request->commands_case);
    TEST_ASSERT_EQUAL_UINT64(0x614233784B397051ull, command_request->deploy->user_id);
    TEST_ASSERT_EQUAL_UINT32(2, command_request->deploy->unit_id);
    TEST_ASSERT_EQUAL_UINT32(6, command_request->deploy->x);
    TEST_ASSERT_EQUAL_UINT32(1, command_request->deploy->y);

    net_server_message__free_unpacked(message, NULL);
}

///
/// Die Anfrage nach den Reserven (NetDiscoverReserveRequest): erkannt wird sie an
/// ihrem Zweig, und alles andere ist sie nicht.
///
void test_message_erkennt_die_reserve_anfrage(void)
{
    NetDiscoverReserveRequest reserve = NET_DISCOVER_RESERVE_REQUEST__INIT;
    NetUserRequest request = NET_USER_REQUEST__INIT;
    request.data_case = NET_USER_REQUEST__DATA_DISCOVER_RESERVE_REQUEST;
    request.discover_reserve_request = &reserve;

    uint8_t buffer[64];
    TEST_ASSERT_TRUE(net_user_request__get_packed_size(&request) <= sizeof(buffer));
    const uint16_t size = (uint16_t)net_user_request__pack(&request, buffer);

    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK, RAD_ParseDiscoverReserveFromMessage(buffer, size));

    // Fuer die anderen Leser ist sie nichts: weder eine Discover-Anfrage noch ein
    // Kommando.
    RAD_DiscoverRequest_t discover;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE, RAD_ParseDiscoverFromMessage(buffer, size, &discover));
    RAD_Command_t command;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE, RAD_ParseCommandFromMessage(buffer, size, &command));

    // Und ein Kommando ist keine Reserve-Anfrage.
    uint8_t deploy[256];
    const uint16_t deploy_size = packe_deploy(deploy, sizeof(deploy), 1, 3, 4, 5);
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE, RAD_ParseDiscoverReserveFromMessage(deploy, deploy_size));
}

void test_message_packt_eine_einheit_der_reserve(void)
{
    RAD_Unit_t unit;
    memset(&unit, 0, sizeof(unit));
    unit.id = 5;
    unit.owner = 0x614233784B397051ull;
    unit.state = RAD_UNIT_STATE_RESERVE;
    // Der laengste Name, den eine Einheit tragen kann.
    memset(unit.name, 'T', RAD_UNIT_NAME_MAX - 1);
    unit.number_of_members = 10;

    // Ein Ringpuffer-Platz (1024 Byte, ZUC_RING_BUFFER_SLOT_SIZE) reicht mit Abstand.
    uint8_t buffer[1024];
    uint16_t size = 0;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK,
        RAD_SerializeReserveUnitEventToMessage(&unit, buffer, sizeof(buffer), &size));
    TEST_ASSERT_TRUE(size < 128);

    NetServerMessage *message = net_server_message__unpack(NULL, size, buffer);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_EQUAL_INT(NET_SERVER_MESSAGE__DATA_EVENT, message->data_case);
    TEST_ASSERT_EQUAL_INT(NET_EVENT__EVENT_GAME, message->event->event_case);

    const NetGameEvent *game_event = message->event->game;
    TEST_ASSERT_EQUAL_INT(NET_GAME_EVENT__EVENT_RESERVE_UNIT, game_event->event_case);

    const NetUnit *net_unit = game_event->reserve_unit->unit;
    TEST_ASSERT_EQUAL_UINT32(5, net_unit->unit_id);
    TEST_ASSERT_EQUAL_UINT64(0x614233784B397051ull, net_unit->owner_id);
    TEST_ASSERT_EQUAL_STRING(unit.name, net_unit->name);
    TEST_ASSERT_EQUAL_UINT32(10, net_unit->number_of_members);

    net_server_message__free_unpacked(message, NULL);

    // Zu wenig Platz: abgelehnt, ohne zu schreiben.
    uint8_t klein[8];
    size = 0;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL,
        RAD_SerializeReserveUnitEventToMessage(&unit, klein, sizeof(klein), &size));
    TEST_ASSERT_EQUAL_UINT16(0, size);
}

///
/// Eine aufgestellte Einheit (NetUnitDeployedEvent): dieselbe Einheit wie in der
/// Reserve, dazu das Feld, auf dem sie jetzt steht.
///
void test_message_packt_eine_aufgestellte_einheit(void)
{
    RAD_Unit_t unit;
    memset(&unit, 0, sizeof(unit));
    unit.id = 5;
    unit.owner = 0x614233784B397051ull;
    unit.state = RAD_UNIT_STATE_DEPLOYED;
    memset(unit.name, 'T', RAD_UNIT_NAME_MAX - 1);
    unit.number_of_members = 10;

    uint8_t buffer[1024];
    uint16_t size = 0;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK,
        RAD_SerializeUnitDeployedEventToMessage(&unit, 3, 7, buffer, sizeof(buffer), &size));
    TEST_ASSERT_TRUE(size < 128);

    NetServerMessage *message = net_server_message__unpack(NULL, size, buffer);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_EQUAL_INT(NET_SERVER_MESSAGE__DATA_EVENT, message->data_case);
    TEST_ASSERT_EQUAL_INT(NET_EVENT__EVENT_GAME, message->event->event_case);

    const NetGameEvent *game_event = message->event->game;
    TEST_ASSERT_EQUAL_INT(NET_GAME_EVENT__EVENT_UNIT_DEPLOYED, game_event->event_case);
    TEST_ASSERT_EQUAL_UINT32(3, game_event->unit_deployed->x);
    TEST_ASSERT_EQUAL_UINT32(7, game_event->unit_deployed->y);

    const NetUnit *net_unit = game_event->unit_deployed->unit;
    TEST_ASSERT_NOT_NULL(net_unit);
    TEST_ASSERT_EQUAL_UINT32(5, net_unit->unit_id);
    TEST_ASSERT_EQUAL_UINT64(0x614233784B397051ull, net_unit->owner_id);
    TEST_ASSERT_EQUAL_STRING(unit.name, net_unit->name);
    TEST_ASSERT_EQUAL_UINT32(10, net_unit->number_of_members);

    net_server_message__free_unpacked(message, NULL);

    // Zu wenig Platz: abgelehnt, ohne zu schreiben.
    uint8_t klein[8];
    size = 0;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_BUFFER_TOO_SMALL,
        RAD_SerializeUnitDeployedEventToMessage(&unit, 3, 7, klein, sizeof(klein), &size));
    TEST_ASSERT_EQUAL_UINT16(0, size);
}

///
/// Der Absender vor der Nutzlast (RAD_ParseSenderFromMessage): 8 Byte Big-Endian,
/// vom Backend gesetzt, und dahinter die NetUserRequest.
///
void test_message_trennt_den_absender_ab(void)
{
    uint8_t buffer[256];
    // Wie das Backend sie baut: erst die Spieler-Id, dann was der Client schickte.
    const uint8_t id[8] = { 0x61, 0x42, 0x33, 0x78, 0x4B, 0x39, 0x70, 0x51 };
    memcpy(buffer, id, sizeof(id));
    // Im Kommando steht ein anderer Absender -- er zaehlt nicht.
    const uint16_t size = packe_deploy(buffer + 8, sizeof(buffer) - 8, 0x1, 3, 4, 5);

    RAD_UserId_t sender = RAD_USER_NONE;
    const uint8_t *payload = NULL;
    uint16_t payload_size = 0;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK,
        RAD_ParseSenderFromMessage(buffer, (uint16_t)(size + 8), &sender, &payload, &payload_size));
    TEST_ASSERT_EQUAL_UINT64(0x614233784B397051ull, sender);
    TEST_ASSERT_EQUAL_PTR(buffer + 8, payload);
    TEST_ASSERT_EQUAL_UINT16(size, payload_size);

    // Die Nutzlast ist die Anfrage des Clients, unveraendert lesbar.
    RAD_Command_t command;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK, RAD_ParseCommandFromMessage(payload, payload_size, &command));
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_TYPE_DEPLOY_UNIT, command.header.type);

    // Genau 8 Byte sind ein Absender ohne Nutzlast; weniger sind keiner.
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK, RAD_ParseSenderFromMessage(buffer, 8, &sender, &payload, &payload_size));
    TEST_ASSERT_EQUAL_UINT16(0, payload_size);

    sender = 42;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_NO_SENDER, RAD_ParseSenderFromMessage(buffer, 7, &sender, &payload, &payload_size));
    TEST_ASSERT_EQUAL_UINT64(42, sender);
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_NO_SENDER, RAD_ParseSenderFromMessage(NULL, 16, &sender, &payload, &payload_size));
}

///
/// Die Reserve-Anfrage, wie ein Client sie schickt: genau die zwei Bytes 1A 00
/// (Feld 3 der NetUserRequest, eingebettet und leer). Auszupacken ohne undefiniertes
/// Verhalten -- unter UBSan geprueft --, weil NetDiscoverReserveRequest ein Feld hat
/// (protobuf/discover.proto).
///
void test_message_liest_die_leere_reserve_anfrage(void)
{
    const uint8_t request[] = { 0x1A, 0x00 };

    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK, RAD_ParseDiscoverReserveFromMessage(request, sizeof(request)));

    RAD_DiscoverRequest_t discover;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE,
        RAD_ParseDiscoverFromMessage(request, sizeof(request), &discover));
}

///
/// Das Abgeben auf der Strecke (NetEndTurnCommand): nur ein Kopf, hin wie
/// zurueck.
///
void test_message_liest_ein_end_turn_kommando(void)
{
    NetEndTurnCommand end_turn = NET_END_TURN_COMMAND__INIT;
    end_turn.user_id = 0x614233784B397051ull;

    NetCommandRequest command_request = NET_COMMAND_REQUEST__INIT;
    command_request.id = 11;
    command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_END_TURN;
    command_request.end_turn = &end_turn;

    NetUserRequest request = NET_USER_REQUEST__INIT;
    request.data_case = NET_USER_REQUEST__DATA_COMMAND_REQUEST;
    request.command_request = &command_request;

    uint8_t buffer[64];
    TEST_ASSERT_TRUE(net_user_request__get_packed_size(&request) <= sizeof(buffer));
    const uint16_t size = (uint16_t)net_user_request__pack(&request, buffer);

    RAD_Command_t command;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK, RAD_ParseCommandFromMessage(buffer, size, &command));
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_TYPE_END_TURN, command.header.type);
    TEST_ASSERT_EQUAL_UINT64(11, command.header.sequence);
    TEST_ASSERT_EQUAL_UINT64(0x614233784B397051ull, command.header.user);
}

void test_message_packt_die_antwort_auf_ein_end_turn(void)
{
    RAD_CommandResponse_t response;
    memset(&response, 0, sizeof(response));
    response.command.header.type = RAD_COMMAND_TYPE_END_TURN;
    response.command.header.sequence = 12;
    response.command.header.user = 0x614233784B397051ull;
    response.header = response.command.header;

    uint8_t buffer[256];
    uint16_t size = 0;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK,
        RAD_SerializeCommandResponseToMessage(&response, true, "in Ordnung", buffer, sizeof(buffer), &size));

    NetServerMessage *message = net_server_message__unpack(NULL, size, buffer);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_EQUAL_INT(NET_SERVER_MESSAGE__DATA_COMMAND_RESPONSE, message->data_case);

    const NetCommandResponse *command_response = message->command_response;
    TEST_ASSERT_TRUE(command_response->success);

    const NetCommandRequest *command_request = command_response->command;
    TEST_ASSERT_EQUAL_UINT32(12, command_request->id);
    TEST_ASSERT_EQUAL_INT(NET_COMMAND_REQUEST__COMMANDS_END_TURN, command_request->commands_case);
    TEST_ASSERT_EQUAL_UINT64(0x614233784B397051ull, command_request->end_turn->user_id);

    net_server_message__free_unpacked(message, NULL);
}

///
/// Die Anfrage nach allen Einheiten (NetDiscoverUnitsRequest): erkannt an ihrem
/// Zweig, und weder Reserve-Anfrage noch Kommando.
///
void test_message_erkennt_die_einheiten_anfrage(void)
{
    NetDiscoverUnitsRequest units = NET_DISCOVER_UNITS_REQUEST__INIT;
    NetUserRequest request = NET_USER_REQUEST__INIT;
    request.data_case = NET_USER_REQUEST__DATA_DISCOVER_UNITS_REQUEST;
    request.discover_units_request = &units;

    uint8_t buffer[64];
    TEST_ASSERT_TRUE(net_user_request__get_packed_size(&request) <= sizeof(buffer));
    const uint16_t size = (uint16_t)net_user_request__pack(&request, buffer);

    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK, RAD_ParseDiscoverUnitsFromMessage(buffer, size));
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE, RAD_ParseDiscoverReserveFromMessage(buffer, size));
    RAD_Command_t command;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE, RAD_ParseCommandFromMessage(buffer, size, &command));

    // Und eine Reserve-Anfrage ist keine Einheiten-Anfrage.
    NetDiscoverReserveRequest reserve = NET_DISCOVER_RESERVE_REQUEST__INIT;
    NetUserRequest reserve_request = NET_USER_REQUEST__INIT;
    reserve_request.data_case = NET_USER_REQUEST__DATA_DISCOVER_RESERVE_REQUEST;
    reserve_request.discover_reserve_request = &reserve;
    const uint16_t reserve_size = (uint16_t)net_user_request__pack(&reserve_request, buffer);
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE, RAD_ParseDiscoverUnitsFromMessage(buffer, reserve_size));
}

void test_message_packt_eine_einheit(void)
{
    RAD_Unit_t unit;
    memset(&unit, 0, sizeof(unit));
    unit.id = 4;
    unit.owner = 0x614233784B397051ull;
    unit.state = RAD_UNIT_STATE_DEPLOYED;
    strcpy(unit.name, "Trupp");
    unit.number_of_members = 3;

    uint8_t buffer[1024];
    uint16_t size = 0;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK,
        RAD_SerializeUnitEventToMessage(&unit, buffer, sizeof(buffer), &size));

    NetServerMessage *message = net_server_message__unpack(NULL, size, buffer);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_EQUAL_INT(NET_SERVER_MESSAGE__DATA_EVENT, message->data_case);
    TEST_ASSERT_EQUAL_INT(NET_EVENT__EVENT_GAME, message->event->event_case);

    const NetGameEvent *game_event = message->event->game;
    TEST_ASSERT_EQUAL_INT(NET_GAME_EVENT__EVENT_UNIT, game_event->event_case);

    const NetUnit *net_unit = game_event->unit->unit;
    TEST_ASSERT_EQUAL_UINT32(4, net_unit->unit_id);
    TEST_ASSERT_EQUAL_UINT64(0x614233784B397051ull, net_unit->owner_id);
    TEST_ASSERT_EQUAL_STRING("Trupp", net_unit->name);
    TEST_ASSERT_EQUAL_UINT32(3, net_unit->number_of_members);

    net_server_message__free_unpacked(message, NULL);
}

///
/// Die Einheit vollstaendig (NetUnit mit Mitgliedern und Waffen): alles, was
/// sie ausmacht, kommt in der einen Nachricht an.
///
void test_message_packt_die_ganze_einheit(void)
{
    RAD_Unit_t unit;
    memset(&unit, 0, sizeof(unit));
    unit.id = 6;
    unit.owner = 0x614233784B397051ull;
    strcpy(unit.name, "Sturmtrupp");
    unit.movement = 4;
    unit.transport_capacity = 2;
    unit.can_capture = true;
    unit.number_of_members = 2;
    // Im laufenden Zug gezogen, aber noch nicht angegriffen.
    unit.turn.moved = 1;

    RAD_UnitMember_t *offizier = &unit.members[0];
    strcpy(offizier->profile, "Offizier");
    offizier->health = 10;
    offizier->armor = 2;
    offizier->strength = 3;
    offizier->accuracy = 4;

    RAD_UnitMember_t *grenadier = &unit.members[1];
    strcpy(grenadier->profile, "Grenadier");
    grenadier->health = 8;
    grenadier->number_of_weapons = 2;
    strcpy(grenadier->weapons[0].name, "Gewehr");
    grenadier->weapons[0].weapon_class = RAD_WEAPON_CLASS_STANDARD;
    grenadier->weapons[0].shots = 2;
    grenadier->weapons[0].strength = 3;
    grenadier->weapons[0].min_range = 0;
    grenadier->weapons[0].max_range = 24;
    grenadier->weapons[0].penetration = 1;
    strcpy(grenadier->weapons[1].name, "Granate");
    grenadier->weapons[1].weapon_class = RAD_WEAPON_CLASS_HEAVY;
    grenadier->weapons[1].min_range = 2;
    grenadier->weapons[1].max_range = 6;

    uint8_t buffer[1024];
    uint16_t size = 0;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK,
        RAD_SerializeUnitEventToMessage(&unit, buffer, sizeof(buffer), &size));

    NetServerMessage *message = net_server_message__unpack(NULL, size, buffer);
    TEST_ASSERT_NOT_NULL(message);
    const NetUnit *net_unit = message->event->game->unit->unit;

    TEST_ASSERT_EQUAL_UINT32(6, net_unit->unit_id);
    TEST_ASSERT_EQUAL_STRING("Sturmtrupp", net_unit->name);
    TEST_ASSERT_EQUAL_UINT32(4, net_unit->movement);
    TEST_ASSERT_EQUAL_UINT32(2, net_unit->transport_capacity);
    TEST_ASSERT_TRUE(net_unit->can_capture);
    TEST_ASSERT_EQUAL_size_t(2, net_unit->n_members);
    TEST_ASSERT_FALSE(net_unit->deployed);
    TEST_ASSERT_TRUE(net_unit->moved);
    TEST_ASSERT_FALSE(net_unit->attacked);

    const NetUnitMember *erstes = net_unit->members[0];
    TEST_ASSERT_EQUAL_STRING("Offizier", erstes->profile);
    TEST_ASSERT_EQUAL_UINT32(10, erstes->health);
    TEST_ASSERT_EQUAL_UINT32(2, erstes->armor);
    TEST_ASSERT_EQUAL_UINT32(3, erstes->strength);
    TEST_ASSERT_EQUAL_UINT32(4, erstes->accuracy);
    TEST_ASSERT_EQUAL_size_t(0, erstes->n_weapons);

    const NetUnitMember *zweites = net_unit->members[1];
    TEST_ASSERT_EQUAL_STRING("Grenadier", zweites->profile);
    TEST_ASSERT_EQUAL_size_t(2, zweites->n_weapons);
    TEST_ASSERT_EQUAL_STRING("Gewehr", zweites->weapons[0]->name);
    TEST_ASSERT_EQUAL_UINT32(2, zweites->weapons[0]->shots);
    TEST_ASSERT_EQUAL_UINT32(3, zweites->weapons[0]->strength);
    TEST_ASSERT_EQUAL_UINT32(24, zweites->weapons[0]->max_range);
    TEST_ASSERT_EQUAL_UINT32(1, zweites->weapons[0]->penetration);
    TEST_ASSERT_EQUAL_STRING("Granate", zweites->weapons[1]->name);
    TEST_ASSERT_EQUAL_UINT32(RAD_WEAPON_CLASS_HEAVY, zweites->weapons[1]->weapon_class);
    TEST_ASSERT_EQUAL_UINT32(2, zweites->weapons[1]->min_range);
    TEST_ASSERT_EQUAL_UINT32(6, zweites->weapons[1]->max_range);

    net_server_message__free_unpacked(message, NULL);
}

///
/// Der Angriff auf der Strecke (NetAttackCommand, protobuf/command.proto): aus
/// einer Nachricht wird ein RAD_CommandAttack_t mit Zielfeld, und die Antwort
/// traegt es zurueck.
///

/// Packt einen Angriff, wie ein Client ihn schickt, nach "buffer"; liefert die Laenge.
static uint16_t packe_angriff(uint8_t *buffer, size_t capacity, uint64_t user, uint32_t unit, uint32_t x, uint32_t y)
{
    NetAttackCommand attack = NET_ATTACK_COMMAND__INIT;
    attack.user_id = user;
    attack.unit_id = unit;
    attack.x = x;
    attack.y = y;

    NetCommandRequest command_request = NET_COMMAND_REQUEST__INIT;
    command_request.id = 11;
    command_request.commands_case = NET_COMMAND_REQUEST__COMMANDS_ATTACK;
    command_request.attack = &attack;

    NetUserRequest request = NET_USER_REQUEST__INIT;
    request.data_case = NET_USER_REQUEST__DATA_COMMAND_REQUEST;
    request.command_request = &command_request;

    const size_t size = net_user_request__get_packed_size(&request);
    TEST_ASSERT_TRUE(size <= capacity);
    return (uint16_t)net_user_request__pack(&request, buffer);
}

void test_message_liest_ein_attack_kommando(void)
{
    uint8_t buffer[256];
    const uint16_t size = packe_angriff(buffer, sizeof(buffer), 0x614233784B397051ull, 3, 6, 2);

    RAD_Command_t command;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK, RAD_ParseCommandFromMessage(buffer, size, &command));

    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_TYPE_ATTACK, command.header.type);
    TEST_ASSERT_EQUAL_UINT64(11, command.header.sequence);
    TEST_ASSERT_EQUAL_UINT64(0x614233784B397051ull, command.header.user);
    TEST_ASSERT_EQUAL_INT(3, command.command.attack.unit);
    TEST_ASSERT_EQUAL_INT(6, command.command.attack.x);
    TEST_ASSERT_EQUAL_INT(2, command.command.attack.y);
}

void test_message_lehnt_ein_angriffsziel_jenseits_von_int16_ab(void)
{
    uint8_t buffer[256];
    const uint16_t size = packe_angriff(buffer, sizeof(buffer), 1, 3, 0, 40000);

    RAD_Command_t command;
    memset(&command, 0x5A, sizeof(command));
    RAD_Command_t vorher = command;

    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_ERROR_UNEXPECTED_MESSAGE, RAD_ParseCommandFromMessage(buffer, size, &command));
    TEST_ASSERT_EQUAL_MEMORY(&vorher, &command, sizeof(command));
}

void test_message_packt_die_antwort_auf_ein_attack(void)
{
    RAD_CommandResponse_t response;
    memset(&response, 0, sizeof(response));
    response.command.header.type = RAD_COMMAND_TYPE_ATTACK;
    response.command.header.sequence = 12;
    response.command.header.user = 0x614233784B397051ull;
    response.command.command.attack.unit = 4;
    response.command.command.attack.x = 5;
    response.command.command.attack.y = 7;
    response.header = response.command.header;

    uint8_t buffer[256];
    uint16_t size = 0;
    TEST_ASSERT_EQUAL_INT(RAD_NET_CODEC_OK,
        RAD_SerializeCommandResponseToMessage(&response, false, "Einheit hat in diesem Zug schon angegriffen",
                                              buffer, sizeof(buffer), &size));

    NetServerMessage *message = net_server_message__unpack(NULL, size, buffer);
    TEST_ASSERT_NOT_NULL(message);
    TEST_ASSERT_EQUAL_INT(NET_SERVER_MESSAGE__DATA_COMMAND_RESPONSE, message->data_case);

    const NetCommandResponse *command_response = message->command_response;
    TEST_ASSERT_FALSE(command_response->success);
    TEST_ASSERT_EQUAL_STRING("Einheit hat in diesem Zug schon angegriffen", command_response->description);

    const NetCommandRequest *command_request = command_response->command;
    TEST_ASSERT_EQUAL_UINT32(12, command_request->id);
    TEST_ASSERT_EQUAL_INT(NET_COMMAND_REQUEST__COMMANDS_ATTACK, command_request->commands_case);
    TEST_ASSERT_EQUAL_UINT64(0x614233784B397051ull, command_request->attack->user_id);
    TEST_ASSERT_EQUAL_UINT32(4, command_request->attack->unit_id);
    TEST_ASSERT_EQUAL_UINT32(5, command_request->attack->x);
    TEST_ASSERT_EQUAL_UINT32(7, command_request->attack->y);

    net_server_message__free_unpacked(message, NULL);
}
