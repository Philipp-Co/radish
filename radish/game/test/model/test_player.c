#include <unity.h>
#include <radish/game/game_definitions.h>
#include <radish/game/model/unit_pool/unit_pool.h>
#include <radish/game/model/reserve/reserve.h>
#include <radish/game/model/player/player.h>

///
/// Der Spieler fuer sich (model/player/player.h): Id, Einheitenliste, Reserve.
/// Die Einheiten kommen aus einem Pool.
///

void test_player_faengt_leer_an(void)
{
    RAD_Player_t *player = RAD_CreatePlayer(42);
    TEST_ASSERT_NOT_NULL(player);
    TEST_ASSERT_EQUAL_UINT64(42, RAD_PlayerId(player));
    TEST_ASSERT_EQUAL_INT32(0, RAD_PlayerNumberOfUnits(player));
    TEST_ASSERT_NOT_NULL(RAD_PlayerReserve(player));
    TEST_ASSERT_EQUAL_INT32(0, RAD_ReserveNumberOfUnits(RAD_PlayerReserve(player)));

    RAD_DestroyPlayer(&player);
    TEST_ASSERT_NULL(player);

    RAD_DestroyPlayer(&player);
    RAD_DestroyPlayer(NULL);
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, RAD_PlayerId(NULL));
    TEST_ASSERT_NULL(RAD_PlayerReserve(NULL));
    TEST_ASSERT_EQUAL_INT32(0, RAD_PlayerNumberOfUnits(NULL));
}

void test_player_ohne_benutzer_gibt_es_nicht(void)
{
    TEST_ASSERT_NULL(RAD_CreatePlayer(RAD_USER_NONE));
}

void test_player_einheitenliste(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();
    RAD_Player_t *player = RAD_CreatePlayer(7);
    RAD_Unit_t *a = RAD_UnitPoolAddUnit(pool);
    RAD_Unit_t *b = RAD_UnitPoolAddUnit(pool);
    RAD_Unit_t *other = RAD_UnitPoolAddUnit(pool);

    TEST_ASSERT_TRUE(RAD_PlayerAddUnit(player, a));
    TEST_ASSERT_TRUE(RAD_PlayerAddUnit(player, b));
    TEST_ASSERT_TRUE(RAD_PlayerAddUnit(player, a));
    TEST_ASSERT_EQUAL_INT32(2, RAD_PlayerNumberOfUnits(player));
    TEST_ASSERT_TRUE(RAD_PlayerUnitAt(player, 0) == a);
    TEST_ASSERT_TRUE(RAD_PlayerUnitAt(player, 1) == b);
    TEST_ASSERT_NULL(RAD_PlayerUnitAt(player, 2));
    TEST_ASSERT_NULL(RAD_PlayerUnitAt(player, -1));
    TEST_ASSERT_TRUE(RAD_PlayerOwnsUnit(player, a));
    TEST_ASSERT_FALSE(RAD_PlayerOwnsUnit(player, other));

    // Die Liste fasst den Besitzer in der Einheit nicht an.
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, a->owner);

    TEST_ASSERT_TRUE(RAD_PlayerRemoveUnit(player, a));
    TEST_ASSERT_TRUE(RAD_PlayerUnitAt(player, 0) == b);
    TEST_ASSERT_FALSE(RAD_PlayerRemoveUnit(player, a));
    TEST_ASSERT_FALSE(RAD_PlayerRemoveUnit(player, other));

    TEST_ASSERT_FALSE(RAD_PlayerAddUnit(player, NULL));
    TEST_ASSERT_FALSE(RAD_PlayerAddUnit(NULL, a));
    TEST_ASSERT_FALSE(RAD_PlayerRemoveUnit(player, NULL));
    TEST_ASSERT_FALSE(RAD_PlayerOwnsUnit(NULL, b));
    TEST_ASSERT_EQUAL_INT32(1, RAD_PlayerNumberOfUnits(player));

    RAD_DestroyPlayer(&player);
    RAD_DestroyUnitPool(&pool);
}

void test_player_entfernen_nimmt_auch_aus_der_reserve(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();
    RAD_Player_t *player = RAD_CreatePlayer(7);
    RAD_Reserve_t *reserve = RAD_PlayerReserve(player);
    RAD_Unit_t *unit = RAD_UnitPoolAddUnit(pool);

    TEST_ASSERT_TRUE(RAD_PlayerAddUnit(player, unit));
    TEST_ASSERT_TRUE(RAD_ReserveAddUnit(reserve, unit));

    TEST_ASSERT_TRUE(RAD_PlayerRemoveUnit(player, unit));
    TEST_ASSERT_FALSE(RAD_ReserveContains(reserve, unit));
    TEST_ASSERT_EQUAL_INT32(0, RAD_ReserveNumberOfUnits(reserve));

    // Zerstoeren laesst den Pool stehen.
    TEST_ASSERT_TRUE(RAD_PlayerAddUnit(player, unit));
    RAD_DestroyPlayer(&player);
    TEST_ASSERT_EQUAL_INT32(1, RAD_UnitPoolNumberOfUnits(pool));

    RAD_DestroyUnitPool(&pool);
}

void test_player_lehnt_ab_wenn_die_liste_voll_ist(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();
    RAD_UnitPool_t *more = RAD_CreateUnitPool();
    RAD_Player_t *player = RAD_CreatePlayer(7);

    for(int32_t i = 0; i < RAD_MAX_UNITS; ++i)
    {
        TEST_ASSERT_TRUE(RAD_PlayerAddUnit(player, RAD_UnitPoolAddUnit(pool)));
    }
    TEST_ASSERT_FALSE(RAD_PlayerAddUnit(player, RAD_UnitPoolAddUnit(more)));
    TEST_ASSERT_EQUAL_INT32(RAD_MAX_UNITS, RAD_PlayerNumberOfUnits(player));

    RAD_DestroyPlayer(&player);
    RAD_DestroyUnitPool(&more);
    RAD_DestroyUnitPool(&pool);
}
