#include <unity.h>
#include <string.h>

#include <radish/game/game.h>
#include <radish/game/model/unit/unit.h>
#include <radish/game/model/tile/tile.h>
#include <radish/server/control/execute.h>
#include <radish/server/control/game_start.h>

///
/// Das Aufstellen durch die Steuerung (RAD_ControlExecuteCommand mit
/// RAD_COMMAND_TYPE_DEPLOY_UNIT): erst der Spielstart, dann das Kommando, und in
/// der Antwort steht, warum es nicht ging.
///

static RAD_EventManager_t *events;
static RAD_Game_t *game;
static RAD_Control_t control;

static RAD_UserId_t host;
static RAD_UserId_t gast;

/// Die Ids der Einheiten in der Reihenfolge des Spielstarts: erst die des Hosts.
static RAD_UnitId_t eigene;
static RAD_UnitId_t fremde;

static void aufbauen(void)
{
    events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);
    game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);
    control = RAD_CreateControl(game);
    TEST_ASSERT_NOT_NULL(control);

    RAD_ControlGameStart_t *start = RAD_ControlCreateGameStart();
    TEST_ASSERT_NOT_NULL(start);
    strcpy(start->players[0].identifier, "aB3xK9pQ");
    strcpy(start->players[1].identifier, "Zz1Yy2Xx");
    for(int32_t p=0;p < RAD_CONTROL_GAME_START_NUMBER_OF_PLAYERS; ++p)
    {
        RAD_Unit_t *unit = &start->players[p].units[0];
        strcpy(unit->name, "Trupp");
        unit->number_of_members = 1;
        strcpy(unit->members[0].profile, "Soldat");
        start->players[p].number_of_units = 1;
    }
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_ControlStartGame(control, start));
    RAD_ControlDestroyGameStart(&start);

    host = RAD_ControlUserIdFromIdentifier("aB3xK9pQ");
    gast = RAD_ControlUserIdFromIdentifier("Zz1Yy2Xx");
    eigene = RAD_GameUserUnitAt(game, host, 0);
    fremde = RAD_GameUserUnitAt(game, gast, 0);
}

static void abbauen(void)
{
    RAD_DestroyControl(&control);
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

static RAD_Command_t aufstellen(RAD_UserId_t user, RAD_UnitId_t unit, int16_t x, int16_t y)
{
    RAD_Command_t command;
    memset(&command, 0, sizeof(command));
    command.header.type = RAD_COMMAND_TYPE_DEPLOY_UNIT;
    command.header.sequence = 1;
    command.header.user = user;
    command.command.deploy_unit.unit = unit;
    command.command.deploy_unit.x = x;
    command.command.deploy_unit.y = y;
    return command;
}

static RAD_Unit_t einheit(RAD_UnitId_t id)
{
    for(int32_t i=0;i < RAD_GameNumberOfUnits(game); ++i)
    {
        RAD_Unit_t unit;
        TEST_ASSERT_TRUE(RAD_GameUnitAt(game, i, &unit));
        if(unit.id == id)
        {
            return unit;
        }
    }
    TEST_FAIL_MESSAGE("Einheit nicht gefunden");
    return (RAD_Unit_t){ .id = RAD_UNIT_NONE };
}

void test_deploy_ueber_die_steuerung_stellt_auf(void)
{
    aufbauen();

    const RAD_Command_t command = aufstellen(host, eigene, 2, 3);
    const RAD_CommandResponse_t response = RAD_ControlExecuteCommand(control, &command);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_OK, response.value);

    const RAD_Unit_t unit = einheit(eigene);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DEPLOYED, unit.state);
    TEST_ASSERT_EQUAL_INT(2, unit.x);
    TEST_ASSERT_EQUAL_INT(3, unit.y);

    RAD_Tile_t tile;
    TEST_ASSERT_TRUE(RAD_ControlTileAt(control, 2, 3, &tile));
    TEST_ASSERT_EQUAL_INT(eigene, tile.unit);

    // Die Antwort traegt das Kommando zurueck, wie bei jedem anderen.
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_TYPE_DEPLOY_UNIT, response.command.header.type);
    TEST_ASSERT_EQUAL_INT(eigene, response.command.command.deploy_unit.unit);

    abbauen();
}

void test_deploy_ueber_die_steuerung_nennt_den_grund(void)
{
    aufbauen();

    // Der Gast ist nicht dran -- das gilt fuer jedes Kommando.
    RAD_Command_t command = aufstellen(gast, fremde, 2, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_NOT_YOUR_TURN, RAD_ControlExecuteCommand(control, &command).value);

    // Die Einheit des Gastes gehoert nicht dem Host.
    command = aufstellen(host, fremde, 2, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_NOT_OWNED, RAD_ControlExecuteCommand(control, &command).value);

    // Eine Einheit, die es nicht gibt.
    command = aufstellen(host, 40, 2, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_NO_SUCH_UNIT, RAD_ControlExecuteCommand(control, &command).value);

    // Ausserhalb der Welt.
    command = aufstellen(host, eigene, -1, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_OUT_OF_BOUNDS, RAD_ControlExecuteCommand(control, &command).value);

    // Abgelehnt heisst: nichts geschrieben.
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, einheit(eigene).state);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, einheit(fremde).state);

    // Steht sie einmal, steht sie nicht mehr in der Reserve.
    command = aufstellen(host, eigene, 2, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_OK, RAD_ControlExecuteCommand(control, &command).value);
    command = aufstellen(host, eigene, 4, 4);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_NOT_IN_RESERVE, RAD_ControlExecuteCommand(control, &command).value);

    // Nach dem Zugwechsel darf der Gast -- aber nicht auf ein besetztes Feld.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, host));
    command = aufstellen(gast, fremde, 2, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_TARGET_OCCUPIED, RAD_ControlExecuteCommand(control, &command).value);
    command = aufstellen(gast, fremde, 5, 5);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_OK, RAD_ControlExecuteCommand(control, &command).value);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DEPLOYED, einheit(fremde).state);

    abbauen();
}

///
/// Was die Reserve-Anfrage zu sehen bekommt (RAD_ControlNumberOfUnits,
/// RAD_ControlUnitAt): alle Einheiten, aufsteigend nach Id, mit ihrem Zustand --
/// in der Reserve steht danach nur, was noch nicht aufgestellt ist.
///
void test_deploy_steuerung_zeigt_die_einheiten(void)
{
    aufbauen();

    TEST_ASSERT_EQUAL_INT(2, RAD_ControlNumberOfUnits(control));

    RAD_Unit_t unit;
    TEST_ASSERT_TRUE(RAD_ControlUnitAt(control, 0, &unit));
    TEST_ASSERT_EQUAL_INT(eigene, unit.id);
    TEST_ASSERT_EQUAL_UINT64(host, unit.owner);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, unit.state);
    TEST_ASSERT_TRUE(RAD_ControlUnitAt(control, 1, &unit));
    TEST_ASSERT_EQUAL_INT(fremde, unit.id);
    TEST_ASSERT_FALSE(RAD_ControlUnitAt(control, 2, &unit));

    const RAD_Command_t command = aufstellen(host, eigene, 2, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_OK, RAD_ControlExecuteCommand(control, &command).value);

    TEST_ASSERT_EQUAL_INT(2, RAD_ControlNumberOfUnits(control));
    TEST_ASSERT_TRUE(RAD_ControlUnitAt(control, 0, &unit));
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DEPLOYED, unit.state);
    TEST_ASSERT_TRUE(RAD_ControlUnitAt(control, 1, &unit));
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, unit.state);

    abbauen();
}
