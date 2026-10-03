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
