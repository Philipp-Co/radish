#include <unity.h>
#include <string.h>
#include <radish/game/game.h>

// Der Test gehoert zum Modul und sieht deshalb den Spielzustand: er prueft, was in
// der Welt angekommen ist. Die Begruendung fuer diesen Suchpfad steht in
// test/CMakeLists.txt.
#include <radish/game/model/game.h>

///
/// Die Regel vor der Reserve (RAD_GameAddUnit, game.h): eine Einheit bekommt nur,
/// wer mitspielt, und nur, solange das Spiel in der Aufstellung ist.
///

static const RAD_UserId_t spieler = (RAD_UserId_t)0x4711;

static RAD_EventManager_t *events;
static RAD_Game_t *game;

static void aufbauen(void)
{
    events = RAD_CreateEventManager();
    TEST_ASSERT_NOT_NULL(events);
    game = RAD_CreateGame(events, RAD_USER_NONE);
    TEST_ASSERT_NOT_NULL(game);
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddPlayer(game, spieler));
}

static void abbauen(void)
{
    RAD_DestroyGame(&game);
    RAD_DestroyEventManager(&events);
}

static RAD_Unit_t trupp(void)
{
    RAD_Unit_t values;
    memset(&values, 0, sizeof(values));
    strcpy(values.name, "Trupp");
    values.number_of_members = 1;
    strcpy(values.members[0].profile, "Soldat");
    return values;
}

void test_einheit_kommt_in_die_reserve_des_spielers(void)
{
    aufbauen();

    const RAD_Unit_t values = trupp();
    RAD_UnitId_t id = RAD_UNIT_NONE;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, spieler, &values, &id));
    TEST_ASSERT_NOT_EQUAL(RAD_UNIT_NONE, id);

    // Sie gehoert ihm und steht in seiner Liste -- auf dem Feld noch nicht.
    TEST_ASSERT_EQUAL_UINT64(spieler, RAD_GameUnitOwner(game, id));
    TEST_ASSERT_EQUAL_INT(1, RAD_GameNumberOfUserUnits(game, spieler));
    TEST_ASSERT_EQUAL_INT(id, RAD_GameUserUnitAt(game, spieler, 0));

    RAD_Unit_t unit;
    TEST_ASSERT_TRUE(RAD_GameUnitAt(game, 0, &unit));
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, unit.state);
    TEST_ASSERT_EQUAL_STRING("Trupp", unit.name);

    // "id" darf NULL sein.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, spieler, &values, NULL));
    TEST_ASSERT_EQUAL_INT(2, RAD_GameNumberOfUserUnits(game, spieler));

    abbauen();
}

void test_einheit_nur_fuer_wer_mitspielt(void)
{
    aufbauen();

    const RAD_Unit_t values = trupp();
    RAD_UnitId_t id = 99;

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NO_USER, RAD_GameAddUnit(game, RAD_USER_NONE, &values, &id));
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_NONE, id);

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_NOT_PLAYING, RAD_GameAddUnit(game, (RAD_UserId_t)0x9999, &values, &id));
    TEST_ASSERT_EQUAL_INT(0, RAD_GameNumberOfUnits(game));

    abbauen();
}

void test_einheit_mit_unhaltbaren_werten_wird_abgelehnt(void)
{
    aufbauen();

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_INVALID_UNIT, RAD_GameAddUnit(game, spieler, NULL, NULL));

    RAD_Unit_t ohne_mitglieder = trupp();
    ohne_mitglieder.number_of_members = 0;
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_INVALID_UNIT, RAD_GameAddUnit(game, spieler, &ohne_mitglieder, NULL));

    TEST_ASSERT_EQUAL_INT(0, RAD_GameNumberOfUnits(game));

    abbauen();
}

void test_einheit_wenn_der_pool_voll_ist(void)
{
    aufbauen();

    const RAD_Unit_t values = trupp();
    int32_t added = 0;
    while(RAD_GameAddUnit(game, spieler, &values, NULL) == RAD_GAME_OK)
    {
        added++;
        TEST_ASSERT_TRUE(added <= 1000);
    }

    // Voll und nicht "ungueltig": dieselben Werte gingen eben noch.
    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_FULL, RAD_GameAddUnit(game, spieler, &values, NULL));
    TEST_ASSERT_EQUAL_INT(added, RAD_GameNumberOfUnits(game));

    abbauen();
}

void test_einheit_nach_dem_ersten_zug_wird_abgelehnt(void)
{
    aufbauen();

    const RAD_Unit_t values = trupp();
    TEST_ASSERT_FALSE(RAD_GameHasStarted(game));
    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameAddUnit(game, spieler, &values, NULL));

    // Ein abgelehntes Zugende schliesst die Aufstellung nicht.
    TEST_ASSERT_NOT_EQUAL(RAD_GAME_OK, RAD_GameEndTurn(game, (RAD_UserId_t)0x9999));
    TEST_ASSERT_FALSE(RAD_GameHasStarted(game));

    TEST_ASSERT_EQUAL_INT(RAD_GAME_OK, RAD_GameEndTurn(game, spieler));
    TEST_ASSERT_TRUE(RAD_GameHasStarted(game));

    TEST_ASSERT_EQUAL_INT(RAD_GAME_ERROR_STARTED, RAD_GameAddUnit(game, spieler, &values, NULL));
    TEST_ASSERT_EQUAL_INT(1, RAD_GameNumberOfUnits(game));

    abbauen();
}
