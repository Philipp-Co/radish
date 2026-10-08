#include <unity.h>
#include <string.h>

#include <radish/game/game.h>
#include <radish/game/model/unit/unit.h>
#include <radish/game/model/tile/tile.h>
#include <radish/server/control/execute.h>
#include <radish/game/control/start_game.h>
#include <radish/server/control/loader.h>

///
/// Das Aufstellen durch die Steuerung (RAD_ControlExecuteCommand mit
/// RAD_COMMAND_TYPE_DEPLOY_UNIT): erst der Spielstart, dann das Kommando, und in
/// der Antwort steht, warum es nicht ging.
///

/// Das Spiel kommt aus dem Loader, wie im Server -- damit ist auch geprueft, dass
/// er die Einheiten-Ereignisse abonniert. "game" ist nur ein kuerzerer Name.
static RAD_ControlGame_t loaded;
static RAD_Game_t *game;
static RAD_Control_t control;

/// Was das Spiel als aufgestellt gemeldet hat (aufgestellt), seit aufbauen.
static int32_t gemeldet;
static RAD_Unit_t gemeldete_einheit;
static int32_t gemeldet_x;
static int32_t gemeldet_y;

static void aufgestellt(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)user_argument;
    gemeldet++;
    gemeldete_einheit = *unit;
    gemeldet_x = x;
    gemeldet_y = y;
}

static void einheit_ohne_belang(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)user_argument; (void)unit; (void)x; (void)y;
}

static void zug_ohne_belang(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    (void)user_argument; (void)unit; (void)path; (void)result;
}

static void zugwechsel_ohne_belang(void *user_argument, RAD_UserId_t current_user)
{
    (void)user_argument; (void)current_user;
}

static void feld_ohne_belang(void *user_argument, const RAD_Tile_t *tile)
{
    (void)user_argument; (void)tile;
}

static RAD_UserId_t host;
static RAD_UserId_t gast;

/// Die Ids der Einheiten in der Reihenfolge des Spielstarts: erst die des Hosts.
static RAD_UnitId_t eigene;
static RAD_UnitId_t fremde;

static void aufbauen(void)
{
    RAD_EventCallbacks_t callbacks = {
        .tile_changed = {
            .user_argument = NULL,
            .added = feld_ohne_belang,
            .removed = feld_ohne_belang,
            .changed = feld_ohne_belang
        },
        .unit_changed = {
            .user_argument = NULL,
            .spawned = aufgestellt,
            .destroyed = einheit_ohne_belang,
            .moved = zug_ohne_belang
        },
        .turn_changed = {
            .user_argument = NULL,
            .changed = zugwechsel_ohne_belang
        }
    };
    loaded = RAD_ControlCreateGame(NULL, &callbacks);
    game = loaded.game;
    TEST_ASSERT_NOT_NULL(game);
    control = RAD_CreateControl(game);
    TEST_ASSERT_NOT_NULL(control);

    RAD_GameStart_t *start = RAD_CreateGameStart();
    TEST_ASSERT_NOT_NULL(start);
    strcpy(start->players[0].identifier, "aB3xK9pQ");
    strcpy(start->players[1].identifier, "Zz1Yy2Xx");
    for(int32_t p=0;p < RAD_GAME_START_NUMBER_OF_PLAYERS; ++p)
    {
        RAD_Unit_t *unit = &start->players[p].units[0];
        strcpy(unit->name, "Trupp");
        unit->number_of_members = 1;
        strcpy(unit->members[0].profile, "Soldat");
        // Eine Waffe, die 4 Felder weit reicht -- fuer die Angriffe unten.
        unit->members[0].number_of_weapons = 1;
        unit->members[0].weapons[0].max_range = 4;
        start->players[p].number_of_units = 1;
    }
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_ControlStartGame(control, start));
    RAD_DestroyGameStart(&start);

    host = RAD_UserIdFromIdentifier("aB3xK9pQ");
    gast = RAD_UserIdFromIdentifier("Zz1Yy2Xx");
    eigene = RAD_GameUserUnitAt(game, host, 0);
    fremde = RAD_GameUserUnitAt(game, gast, 0);

    // Was der Spielstart meldet, zaehlt nicht: gezaehlt wird ab hier.
    gemeldet = 0;
}

static void abbauen(void)
{
    RAD_DestroyControl(&control);
    RAD_ControlDestroyGame(&loaded);
    game = NULL;
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

///
/// Was der Server als NetUnitDeployedEvent hinausschickt: genau eine Meldung je
/// Aufstellen, mit der Einheit im neuen Zustand und ihrem Feld -- und keine fuer
/// ein abgelehntes Kommando.
///
void test_deploy_meldet_die_aufgestellte_einheit(void)
{
    aufbauen();

    RAD_Command_t command = aufstellen(host, eigene, -1, 4);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_OUT_OF_BOUNDS, RAD_ControlExecuteCommand(control, &command).value);
    command = aufstellen(host, fremde, 4, 4);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_NOT_OWNED, RAD_ControlExecuteCommand(control, &command).value);
    TEST_ASSERT_EQUAL_INT(0, gemeldet);

    command = aufstellen(host, eigene, 4, 5);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_OK, RAD_ControlExecuteCommand(control, &command).value);

    TEST_ASSERT_EQUAL_INT(1, gemeldet);
    TEST_ASSERT_EQUAL_INT(eigene, gemeldete_einheit.id);
    TEST_ASSERT_EQUAL_UINT64(host, gemeldete_einheit.owner);
    TEST_ASSERT_EQUAL_STRING("Trupp", gemeldete_einheit.name);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DEPLOYED, gemeldete_einheit.state);
    TEST_ASSERT_EQUAL_INT(4, gemeldet_x);
    TEST_ASSERT_EQUAL_INT(5, gemeldet_y);

    // Ein zweites Mal geht nicht -- und meldet nichts.
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_NOT_IN_RESERVE, RAD_ControlExecuteCommand(control, &command).value);
    TEST_ASSERT_EQUAL_INT(1, gemeldet);

    abbauen();
}

static RAD_Command_t ziehen(RAD_UserId_t user, RAD_UnitId_t unit, int16_t von_x, int16_t von_y, int16_t nach_x, int16_t nach_y)
{
    RAD_Command_t command;
    memset(&command, 0, sizeof(command));
    command.header.type = RAD_COMMAND_TYPE_MOVE_UNIT;
    command.header.sequence = 1;
    command.header.user = user;
    command.command.move_unit.unit = unit;
    command.command.move_unit.path.steps_to[0].x = von_x;
    command.command.move_unit.path.steps_to[0].y = von_y;
    command.command.move_unit.path.steps_to[1].x = nach_x;
    command.command.move_unit.path.steps_to[1].y = nach_y;
    command.command.move_unit.path.number_of_steps = 2;
    return command;
}

static RAD_Command_t angreifen(RAD_UserId_t user, RAD_UnitId_t unit, int16_t x, int16_t y)
{
    RAD_Command_t command;
    memset(&command, 0, sizeof(command));
    command.header.type = RAD_COMMAND_TYPE_ATTACK;
    command.header.sequence = 1;
    command.header.user = user;
    command.command.attack.unit = unit;
    command.command.attack.x = x;
    command.command.attack.y = y;
    return command;
}

///
/// Ziehen und Angreifen durch die Steuerung: im Zug des Aufstellens keins von
/// beiden, danach je einmal -- und in der Antwort steht, warum es nicht ging
/// (RAD_GameCheckMoveUnit, RAD_GameCheckAttack).
///
void test_ziehen_und_angreifen_ueber_die_steuerung_nennt_den_grund(void)
{
    aufbauen();

    RAD_Command_t command = aufstellen(host, eigene, 2, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_OK, RAD_ControlExecuteCommand(control, &command).value);

    // Eine fremde Einheit fasst der Host nicht an -- der Besitz kommt zuerst.
    command = ziehen(host, fremde, 0, 0, 1, 0);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_NOT_OWNED, RAD_ControlExecuteCommand(control, &command).value);
    command = angreifen(host, fremde, 4, 4);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_NOT_OWNED, RAD_ControlExecuteCommand(control, &command).value);

    // Im Zug des Aufstellens weder ziehen noch angreifen.
    command = ziehen(host, eigene, 2, 3, 3, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_UNIT_JUST_DEPLOYED, RAD_ControlExecuteCommand(control, &command).value);
    command = angreifen(host, eigene, 4, 4);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_UNIT_JUST_DEPLOYED, RAD_ControlExecuteCommand(control, &command).value);
    TEST_ASSERT_EQUAL_INT(2, einheit(eigene).x);

    // Eine Runde weiter ist der Host wieder dran.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, host));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, gast));

    command = ziehen(host, eigene, 2, 3, 3, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_OK, RAD_ControlExecuteCommand(control, &command).value);
    TEST_ASSERT_EQUAL_INT(3, einheit(eigene).x);
    command = ziehen(host, eigene, 3, 3, 4, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_UNIT_ALREADY_MOVED, RAD_ControlExecuteCommand(control, &command).value);
    TEST_ASSERT_EQUAL_INT(3, einheit(eigene).x);

    // Ein Ziel ausserhalb der Welt oder der Reichweite zaehlt nicht; danach greift
    // sie einmal an. Sie steht auf (3, 3), ihre Waffe reicht 4 Felder.
    command = angreifen(host, eigene, -1, 3);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_OUT_OF_BOUNDS, RAD_ControlExecuteCommand(control, &command).value);
    command = angreifen(host, eigene, 7, 7);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_TARGET_OUT_OF_RANGE, RAD_ControlExecuteCommand(control, &command).value);
    command = angreifen(host, eigene, 5, 5);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_OK, RAD_ControlExecuteCommand(control, &command).value);
    command = angreifen(host, eigene, 5, 5);
    TEST_ASSERT_EQUAL_UINT32(RAD_CONTROL_ERROR_UNIT_ALREADY_ATTACKED, RAD_ControlExecuteCommand(control, &command).value);

    // Der Text, der ueber die Strecke geht.
    TEST_ASSERT_EQUAL_STRING("Einheit hat in diesem Zug schon angegriffen",
                             RAD_ControlResultText(RAD_CONTROL_ERROR_UNIT_ALREADY_ATTACKED));

    abbauen();
}
