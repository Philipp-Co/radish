#include <unity.h>
#include <radish/game/game_definitions.h>
#include <radish/game/model/unit_pool/unit_pool.h>
#include <radish/game/model/reserve/reserve.h>

///
/// Die Reserve fuer sich (model/reserve/reserve.h). Die Einheiten kommen aus
/// einem Pool, wie im Spiel -- die Reserve legt keine an.
///

void test_reserve_faengt_leer_an(void)
{
    RAD_Reserve_t *reserve = RAD_CreateReserve();
    TEST_ASSERT_NOT_NULL(reserve);
    TEST_ASSERT_EQUAL_INT32(0, RAD_ReserveNumberOfUnits(reserve));
    TEST_ASSERT_NULL(RAD_ReserveUnitAt(reserve, 0));

    RAD_DestroyReserve(&reserve);
    TEST_ASSERT_NULL(reserve);

    RAD_DestroyReserve(&reserve);
    RAD_DestroyReserve(NULL);
    TEST_ASSERT_EQUAL_INT32(0, RAD_ReserveNumberOfUnits(NULL));
}

void test_reserve_haengt_hinten_an_und_rueckt_nach(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();
    RAD_Reserve_t *reserve = RAD_CreateReserve();
    RAD_Unit_t *a = RAD_UnitPoolAddUnit(pool);
    RAD_Unit_t *b = RAD_UnitPoolAddUnit(pool);
    RAD_Unit_t *c = RAD_UnitPoolAddUnit(pool);

    TEST_ASSERT_TRUE(RAD_ReserveAddUnit(reserve, a));
    TEST_ASSERT_TRUE(RAD_ReserveAddUnit(reserve, b));
    TEST_ASSERT_TRUE(RAD_ReserveAddUnit(reserve, c));
    TEST_ASSERT_EQUAL_INT32(3, RAD_ReserveNumberOfUnits(reserve));
    TEST_ASSERT_TRUE(RAD_ReserveUnitAt(reserve, 0) == a);
    TEST_ASSERT_TRUE(RAD_ReserveUnitAt(reserve, 2) == c);
    TEST_ASSERT_NULL(RAD_ReserveUnitAt(reserve, 3));
    TEST_ASSERT_NULL(RAD_ReserveUnitAt(reserve, -1));

    TEST_ASSERT_TRUE(RAD_ReserveRemoveUnit(reserve, b));
    TEST_ASSERT_EQUAL_INT32(2, RAD_ReserveNumberOfUnits(reserve));
    TEST_ASSERT_TRUE(RAD_ReserveUnitAt(reserve, 1) == c);
    TEST_ASSERT_FALSE(RAD_ReserveContains(reserve, b));

    // Zurueck steht sie hinten.
    TEST_ASSERT_TRUE(RAD_ReserveAddUnit(reserve, b));
    TEST_ASSERT_TRUE(RAD_ReserveUnitAt(reserve, 2) == b);

    RAD_DestroyReserve(&reserve);
    RAD_DestroyUnitPool(&pool);
}

void test_reserve_aendert_die_einheit_nicht(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();
    RAD_Reserve_t *reserve = RAD_CreateReserve();
    RAD_Unit_t *unit = RAD_UnitPoolAddUnit(pool);
    unit->state = RAD_UNIT_STATE_DEPLOYED;

    TEST_ASSERT_TRUE(RAD_ReserveAddUnit(reserve, unit));
    TEST_ASSERT_TRUE(RAD_ReserveRemoveUnit(reserve, unit));
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_DEPLOYED, unit->state);

    // Zerstoeren laesst den Pool stehen.
    TEST_ASSERT_TRUE(RAD_ReserveAddUnit(reserve, unit));
    RAD_DestroyReserve(&reserve);
    TEST_ASSERT_EQUAL_INT32(1, RAD_UnitPoolNumberOfUnits(pool));

    RAD_DestroyUnitPool(&pool);
}

void test_reserve_doppelt_und_ungueltig(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();
    RAD_Reserve_t *reserve = RAD_CreateReserve();
    RAD_Unit_t *unit = RAD_UnitPoolAddUnit(pool);
    RAD_Unit_t *outside = RAD_UnitPoolAddUnit(pool);

    TEST_ASSERT_TRUE(RAD_ReserveAddUnit(reserve, unit));
    TEST_ASSERT_TRUE(RAD_ReserveAddUnit(reserve, unit));
    TEST_ASSERT_EQUAL_INT32(1, RAD_ReserveNumberOfUnits(reserve));

    TEST_ASSERT_FALSE(RAD_ReserveAddUnit(reserve, NULL));
    TEST_ASSERT_FALSE(RAD_ReserveAddUnit(NULL, unit));
    TEST_ASSERT_FALSE(RAD_ReserveRemoveUnit(reserve, outside));
    TEST_ASSERT_FALSE(RAD_ReserveRemoveUnit(reserve, NULL));
    TEST_ASSERT_FALSE(RAD_ReserveRemoveUnit(NULL, unit));
    TEST_ASSERT_FALSE(RAD_ReserveContains(reserve, NULL));
    TEST_ASSERT_FALSE(RAD_ReserveContains(NULL, unit));
    TEST_ASSERT_EQUAL_INT32(1, RAD_ReserveNumberOfUnits(reserve));

    RAD_DestroyReserve(&reserve);
    RAD_DestroyUnitPool(&pool);
}

void test_reserven_sind_voneinander_unabhaengig(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();
    RAD_Reserve_t *first = RAD_CreateReserve();
    RAD_Reserve_t *second = RAD_CreateReserve();
    RAD_Unit_t *unit = RAD_UnitPoolAddUnit(pool);

    TEST_ASSERT_TRUE(RAD_ReserveAddUnit(first, unit));
    TEST_ASSERT_TRUE(RAD_ReserveContains(first, unit));
    TEST_ASSERT_FALSE(RAD_ReserveContains(second, unit));
    TEST_ASSERT_EQUAL_INT32(0, RAD_ReserveNumberOfUnits(second));

    RAD_DestroyReserve(&second);
    RAD_DestroyReserve(&first);
    RAD_DestroyUnitPool(&pool);
}

void test_reserve_lehnt_ab_wenn_sie_voll_ist(void)
{
    // Zwei Pools, weil einer allein nicht mehr Einheiten hat, als die Reserve fasst.
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();
    RAD_UnitPool_t *more = RAD_CreateUnitPool();
    RAD_Reserve_t *reserve = RAD_CreateReserve();

    for(int32_t i = 0; i < RAD_MAX_UNITS; ++i)
    {
        TEST_ASSERT_TRUE(RAD_ReserveAddUnit(reserve, RAD_UnitPoolAddUnit(pool)));
    }
    RAD_Unit_t *one_too_many = RAD_UnitPoolAddUnit(more);
    TEST_ASSERT_FALSE(RAD_ReserveAddUnit(reserve, one_too_many));
    TEST_ASSERT_EQUAL_INT32(RAD_MAX_UNITS, RAD_ReserveNumberOfUnits(reserve));

    RAD_DestroyReserve(&reserve);
    RAD_DestroyUnitPool(&more);
    RAD_DestroyUnitPool(&pool);
}
