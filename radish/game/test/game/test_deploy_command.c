#include <unity.h>
#include <string.h>
#include <radish/game/game.h>

// Der Test gehoert zum Modul und sieht deshalb den Spielzustand: er prueft in der
// Welt nach, wo die Einheit danach steht. Die Begruendung fuer diesen Suchpfad
// steht in test/CMakeLists.txt.
#include <radish/game/model/game.h>
#include <radish/game/model/world/world.h>

///
/// Das Aufstellen aus der Reserve (RAD_COMMAND_TYPE_DEPLOY_UNIT): die Fabrik, die
/// Regel (RAD_GameCheckDeployUnit) und was das Kommando im Spiel anrichtet.
///

static const RAD_UserId_t host = (RAD_UserId_t)0x4711;
static const RAD_UserId_t gast = (RAD_UserId_t)0x0815;

static RAD_EventManager_t *events;
static RAD_Game_t *game;
static RAD_UnitId_t eigene;
static RAD_UnitId_t fremde;

typedef struct
{
    int32_t spawned;
    RAD_UnitId_t unit;
    int32_t x;
    int32_t y;
} RAD_AufstellMitschrift_t;

static RAD_AufstellMitschrift_t mitschrift;

static void notiere_spawned(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    RAD_AufstellMitschrift_t *m = user_argument;
    m->spawned++;
    m->unit = unit->id;
    m->x = x;
    m->y = y;
}

static void notiere_nichts(void *user_argument, const RAD_Unit_t *unit, int32_t x, int32_t y)
{
    (void)user_argument;
    (void)unit;
    (void)x;
    (void)y;
}

static void notiere_keinen_zug(void *user_argument, const RAD_Unit_t *unit, const RAD_Path_t *path, int32_t result)
{
    (void)user_argument;
    (void)unit;
    (void)path;
    (void)result;
}

///
/// Zwei Spieler mit je einer Einheit in der Reserve; der Host ist dran. Abonniert
/// wird zuletzt, damit nur zaehlt, was der Test ausloest.
///
static void aufbauen(void)
{
    events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);
    game = RAD_CreateGame(events, host);
    TEST_ASSERT_NOT_NULL(game);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, host));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, gast));

    RAD_Unit_t values;
    memset(&values, 0, sizeof(values));
    strcpy(values.name, "Trupp");
    values.number_of_members = 1;
    strcpy(values.members[0].profile, "Soldat");

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, host, &values, &eigene));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, gast, &values, &fremde));

    memset(&mitschrift, 0, sizeof(mitschrift));
    mitschrift.unit = RAD_UNIT_NONE;
    RAD_EventManagerSubscribeToUnitEvents(events, (RAD_EventsUnitChangedCallback_t){
        .user_argument = &mitschrift,
        .spawned = notiere_spawned,
        .destroyed = notiere_nichts,
        .moved = notiere_keinen_zug
    });
}

static void abbauen(void)
{
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

void test_deploy_fabrik_fuellt_das_kommando(void)
{
    aufbauen();

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameDeployUnit(game, eigene, 3, 4, &command));

    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_TYPE_DEPLOY_UNIT, command.header.type);
    TEST_ASSERT_EQUAL_UINT64(host, command.header.user);
    TEST_ASSERT_EQUAL_UINT64(1, command.header.sequence);
    TEST_ASSERT_EQUAL_INT(eigene, command.command.deploy_unit.unit);
    TEST_ASSERT_EQUAL_INT(3, command.command.deploy_unit.x);
    TEST_ASSERT_EQUAL_INT(4, command.command.deploy_unit.y);

    // Abgelehnt wird ohne "output" anzufassen und ohne eine Nummer zu verbrauchen.
    RAD_Command_t unberuehrt;
    memset(&unberuehrt, 0xAB, sizeof(unberuehrt));
    RAD_Command_t vorher = unberuehrt;
    TEST_ASSERT_FALSE(RAD_GameDeployUnit(game, RAD_UNIT_NONE, 0, 0, &unberuehrt));
    TEST_ASSERT_EQUAL_MEMORY(&vorher, &unberuehrt, sizeof(unberuehrt));
    TEST_ASSERT_FALSE(RAD_GameDeployUnit(game, eigene, 0, 0, NULL));
    TEST_ASSERT_FALSE(RAD_GameDeployUnit(NULL, eigene, 0, 0, &command));

    TEST_ASSERT_TRUE(RAD_GameDeployUnit(game, eigene, 0, 0, &command));
    TEST_ASSERT_EQUAL_UINT64(2, command.header.sequence);

    abbauen();
}

void test_deploy_regel_nennt_jeden_grund(void)
{
    aufbauen();

    TEST_ASSERT_TRUE(RAD_WorldRemoveTile(&game->world, 5, 5));
    const RAD_UnitId_t im_weg = RAD_WorldSpawnUnit(&game->world, RAD_UNIT_TYPE_NPC, 6, 6);
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, im_weg);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK,                   RAD_GameCheckDeployUnit(game, host, eigene, 1, 1));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NO_USER,        RAD_GameCheckDeployUnit(game, RAD_USER_NONE, eigene, 1, 1));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NOT_PLAYING,    RAD_GameCheckDeployUnit(game, (RAD_UserId_t)0x9999, eigene, 1, 1));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NO_UNIT,        RAD_GameCheckDeployUnit(game, host, RAD_UNIT_NONE, 1, 1));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NO_UNIT,        RAD_GameCheckDeployUnit(game, host, 40, 1, 1));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NOT_OWNED,      RAD_GameCheckDeployUnit(game, host, fremde, 1, 1));
    // Herrenlos ist auch nicht "seine" -- strenger als RAD_GameMayControlUnit.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NOT_OWNED,      RAD_GameCheckDeployUnit(game, host, im_weg, 1, 1));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_OUT_OF_BOUNDS,  RAD_GameCheckDeployUnit(game, host, eigene, -1, 0));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_OUT_OF_BOUNDS,  RAD_GameCheckDeployUnit(game, host, eigene, 0, game->world.height));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NO_GROUND,      RAD_GameCheckDeployUnit(game, host, eigene, 5, 5));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_OCCUPIED,       RAD_GameCheckDeployUnit(game, host, eigene, 6, 6));

    // Steht sie, steht sie nicht mehr in der Reserve; zerstoert genauso wenig.
    TEST_ASSERT_TRUE(RAD_WorldDeployUnit(&game->world, eigene, 1, 1));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NOT_IN_RESERVE, RAD_GameCheckDeployUnit(game, host, eigene, 2, 2));
    RAD_WorldRemoveUnit(&game->world, eigene);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NOT_IN_RESERVE, RAD_GameCheckDeployUnit(game, host, eigene, 2, 2));

    abbauen();
}

void test_deploy_kommando_stellt_die_einheit_auf(void)
{
    aufbauen();

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameDeployUnit(game, eigene, 3, 4, &command));
    RAD_GameExecuteCommand(game, &command);

    const RAD_Unit_t *unit = RAD_WorldUnitById(&game->world, eigene);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DEPLOYED, unit->state);
    TEST_ASSERT_EQUAL_INT(3, unit->x);
    TEST_ASSERT_EQUAL_INT(4, unit->y);
    TEST_ASSERT_EQUAL_INT(eigene, RAD_WorldTileAt(&game->world, 3, 4)->unit);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    TEST_ASSERT_EQUAL_INT(1, mitschrift.spawned);
    TEST_ASSERT_EQUAL_INT(eigene, mitschrift.unit);
    TEST_ASSERT_EQUAL_INT(3, mitschrift.x);
    TEST_ASSERT_EQUAL_INT(4, mitschrift.y);

    // Zweimal zugestellt: beim zweiten Mal steht sie schon, und nichts aendert sich.
    command.command.deploy_unit.x = 5;
    RAD_GameExecuteCommand(game, &command);
    TEST_ASSERT_EQUAL_INT(3, unit->x);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldTileAt(&game->world, 5, 4)->unit);
    TEST_ASSERT_EQUAL_INT(1, mitschrift.spawned);

    abbauen();
}

///
/// Das Spiel prueft selbst, auch wenn ein Kommando an der Vorpruefung des Servers
/// vorbei ankommt: eine fremde Einheit stellt niemand auf.
///
void test_deploy_kommando_fuer_fremde_einheit_aendert_nichts(void)
{
    aufbauen();

    RAD_Command_t command = {0};
    TEST_ASSERT_TRUE(RAD_GameDeployUnit(game, fremde, 2, 2, &command));
    TEST_ASSERT_EQUAL_UINT64(host, command.header.user);

    RAD_GameExecuteCommand(game, &command);

    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, RAD_WorldUnitById(&game->world, fremde)->state);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, RAD_WorldTileAt(&game->world, 2, 2)->unit);
    TEST_ASSERT_EQUAL_INT(0, mitschrift.spawned);
    TEST_ASSERT_TRUE(RAD_WorldIsConsistent(&game->world));

    abbauen();
}
