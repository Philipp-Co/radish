#include <unity.h>
#include <radish/game/game_definitions.h>
#include <radish/game/model/unit_pool/unit_pool.h>

///
/// Der Einheitenpool fuer sich: anlegen, Einheiten hinein und heraus, zerstoeren
/// (model/unit_pool/unit_pool.h).
///

void test_unit_pool_faengt_leer_an(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_EQUAL_INT32(0, RAD_UnitPoolNumberOfUnits(pool));

    RAD_DestroyUnitPool(&pool);
    TEST_ASSERT_NULL(pool);

    // Ein zweites Mal, und mit NULL selbst: beides tut nichts.
    RAD_DestroyUnitPool(&pool);
    RAD_DestroyUnitPool(NULL);
    TEST_ASSERT_EQUAL_INT32(0, RAD_UnitPoolNumberOfUnits(NULL));
}

void test_unit_pool_neue_einheit_steht_in_der_reserve(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();

    RAD_Unit_t *first = RAD_UnitPoolAddUnit(pool);
    RAD_Unit_t *second = RAD_UnitPoolAddUnit(pool);
    TEST_ASSERT_NOT_NULL(first);
    TEST_ASSERT_NOT_NULL(second);
    TEST_ASSERT_TRUE(first != second);
    TEST_ASSERT_EQUAL_INT32(2, RAD_UnitPoolNumberOfUnits(pool));

    TEST_ASSERT_EQUAL_INT32(0, first->id);
    TEST_ASSERT_EQUAL_INT32(1, second->id);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, first->state);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_TYPE_NONE, first->type);
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, first->owner);
    TEST_ASSERT_EQUAL_INT16(-1, first->x);
    TEST_ASSERT_EQUAL_INT16(-1, first->y);
    TEST_ASSERT_EQUAL_INT32(0, first->number_of_members);

    // Ueber den Zeiger gefuellt, bleibt es im Pool stehen.
    first->movement = 6;
    TEST_ASSERT_EQUAL_INT16(6, first->movement);

    RAD_DestroyUnitPool(&pool);
}

void test_unit_pool_entfernen_gibt_den_platz_frei_aber_nicht_die_id(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();

    RAD_Unit_t *unit = RAD_UnitPoolAddUnit(pool);
    RAD_Unit_t *const slot = unit;
    TEST_ASSERT_EQUAL_INT32(0, unit->id);

    TEST_ASSERT_TRUE(RAD_UnitPoolRemoveUnit(pool, &unit));
    TEST_ASSERT_NULL(unit);
    TEST_ASSERT_EQUAL_INT32(0, RAD_UnitPoolNumberOfUnits(pool));

    // Derselbe Platz, eine neue Id -- und der alte Stand ist weg.
    RAD_Unit_t *again = RAD_UnitPoolAddUnit(pool);
    TEST_ASSERT_TRUE(again == slot);
    TEST_ASSERT_EQUAL_INT32(1, again->id);
    TEST_ASSERT_EQUAL_INT(RAD_UNIT_STATE_RESERVE, again->state);

    RAD_DestroyUnitPool(&pool);
}

void test_unit_pool_entfernen_weist_fremde_zeiger_ab(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();
    RAD_UnitPool_t *other = RAD_CreateUnitPool();

    RAD_Unit_t *unit = RAD_UnitPoolAddUnit(pool);
    RAD_Unit_t *foreign = RAD_UnitPoolAddUnit(other);
    RAD_Unit_t on_stack = {0};
    RAD_Unit_t *outside = &on_stack;
    RAD_Unit_t *none = NULL;

    TEST_ASSERT_FALSE(RAD_UnitPoolRemoveUnit(pool, &foreign));
    TEST_ASSERT_NOT_NULL(foreign);
    TEST_ASSERT_FALSE(RAD_UnitPoolRemoveUnit(pool, &outside));
    TEST_ASSERT_TRUE(outside == &on_stack);
    TEST_ASSERT_FALSE(RAD_UnitPoolRemoveUnit(pool, &none));
    TEST_ASSERT_FALSE(RAD_UnitPoolRemoveUnit(pool, NULL));
    TEST_ASSERT_FALSE(RAD_UnitPoolRemoveUnit(NULL, &unit));
    TEST_ASSERT_EQUAL_INT32(1, RAD_UnitPoolNumberOfUnits(pool));

    // Ein schon freier Platz ist keine Einheit mehr.
    RAD_Unit_t *stale = unit;
    TEST_ASSERT_TRUE(RAD_UnitPoolRemoveUnit(pool, &unit));
    TEST_ASSERT_FALSE(RAD_UnitPoolRemoveUnit(pool, &stale));
    TEST_ASSERT_EQUAL_INT32(0, RAD_UnitPoolNumberOfUnits(pool));

    RAD_DestroyUnitPool(&other);
    RAD_DestroyUnitPool(&pool);
}

void test_unit_pool_lehnt_ab_wenn_er_voll_ist(void)
{
    RAD_UnitPool_t *pool = RAD_CreateUnitPool();

    for(int32_t i = 0; i < RAD_MAX_UNITS; ++i)
    {
        TEST_ASSERT_NOT_NULL(RAD_UnitPoolAddUnit(pool));
    }
    TEST_ASSERT_NULL(RAD_UnitPoolAddUnit(pool));
    TEST_ASSERT_EQUAL_INT32(RAD_MAX_UNITS, RAD_UnitPoolNumberOfUnits(pool));
    TEST_ASSERT_NULL(RAD_UnitPoolAddUnit(NULL));

    RAD_DestroyUnitPool(&pool);
}
