#include <unity.h>
#include <string.h>

#include <radish/game/game.h>
#include <radish/game/model/unit/unit.h>
#include <radish/server/control/execute.h>
#include <radish/server/control/game_start.h>

///
/// Der Spielstart im Spiel (RAD_ControlStartGame, control/execute.h): beide
/// Spieler spielen mit, ihre Armeen stehen in der Reserve -- und das genau einmal.
///
/// Der Spielstart wird hier von Hand gebaut und nicht aus einer Datei gelesen: was
/// der Leser kann, prueft test_game_start.c, hier geht es um das, was danach im
/// Spiel steht.
///

static RAD_EventManager_t *events;
static RAD_Game_t *game;
static RAD_Control_t control;
static RAD_ControlGameStart_t *start;

static void einheit(RAD_ControlGameStartPlayer_t *player, const char *name)
{
    RAD_Unit_t *unit = &player->units[player->number_of_units++];
    memset(unit, 0, sizeof(*unit));
    strcpy(unit->name, name);
    unit->movement = 6;
    unit->number_of_members = 1;
    strcpy(unit->members[0].profile, "Soldat");
}

static void aufbauen(void)
{
    events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);
    game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);
    control = RAD_CreateControl(game);
    TEST_ASSERT_NOT_NULL(control);

    start = RAD_ControlCreateGameStart();
    TEST_ASSERT_NOT_NULL(start);

    strcpy(start->players[0].identifier, "aB3xK9pQ");
    einheit(&start->players[0], "Trupp");
    einheit(&start->players[0], "Transporter");

    strcpy(start->players[1].identifier, "Zz1Yy2Xx");
    einheit(&start->players[1], "Trupp");
}

static void abbauen(void)
{
    RAD_ControlDestroyGameStart(&start);
    RAD_DestroyControl(&control);
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

void test_spielstart_traegt_spieler_und_armeen_ein(void)
{
    aufbauen();

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_ControlStartGame(control, start));

    const RAD_UserId_t host = RAD_ControlUserIdFromIdentifier("aB3xK9pQ");
    const RAD_UserId_t zweiter = RAD_ControlUserIdFromIdentifier("Zz1Yy2Xx");

    // Beide spielen mit, der Host zuerst -- und damit ist er auch dran.
    TEST_ASSERT_EQUAL_INT(2, RAD_GameNumberOfPlayers(game));
    TEST_ASSERT_EQUAL_UINT64(host, RAD_GamePlayerAt(game, 0));
    TEST_ASSERT_EQUAL_UINT64(zweiter, RAD_GamePlayerAt(game, 1));
    TEST_ASSERT_EQUAL_UINT64(host, RAD_GameCurrentUser(game));

    // Ihre Einheiten gehoeren ihnen und stehen in der Reserve, in der Reihenfolge
    // der Armee.
    TEST_ASSERT_EQUAL_INT(2, RAD_GameNumberOfUserUnits(game, host));
    TEST_ASSERT_EQUAL_INT(1, RAD_GameNumberOfUserUnits(game, zweiter));
    TEST_ASSERT_EQUAL_INT(3, RAD_GameNumberOfUnits(game));

    const char *const namen[] = { "Trupp", "Transporter", "Trupp" };
    const RAD_UserId_t besitzer[] = { host, host, zweiter };
    for(int32_t i=0;i < 3; ++i)
    {
        RAD_Unit_t unit;
        TEST_ASSERT_TRUE(RAD_GameUnitAt(game, i, &unit));
        TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, unit.state);
        TEST_ASSERT_EQUAL_STRING(namen[i], unit.name);
        TEST_ASSERT_EQUAL_UINT64(besitzer[i], unit.owner);
        TEST_ASSERT_EQUAL_INT(-1, unit.x);
    }

    abbauen();
}

void test_spielstart_geht_nur_einmal(void)
{
    aufbauen();

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_ControlStartGame(control, start));

    // Dasselbe Signal noch einmal: nichts kommt dazu.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_STARTED, RAD_ControlStartGame(control, start));
    TEST_ASSERT_EQUAL_INT(3, RAD_GameNumberOfUnits(game));
    TEST_ASSERT_EQUAL_INT(2, RAD_GameNumberOfPlayers(game));

    abbauen();
}

void test_spielstart_mit_ungueltiger_kennung_aendert_nichts(void)
{
    aufbauen();

    strcpy(start->players[1].identifier, "kaputt!");
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NO_USER, RAD_ControlStartGame(control, start));

    // Geprueft wird vor dem ersten Eintrag -- auch der gueltige Host ist nicht drin.
    TEST_ASSERT_EQUAL_INT(0, RAD_GameNumberOfPlayers(game));
    TEST_ASSERT_EQUAL_INT(0, RAD_GameNumberOfUnits(game));

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_INVALID_UNIT, RAD_ControlStartGame(control, NULL));

    abbauen();
}
