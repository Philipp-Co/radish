#include <unity.h>
#include <math.h>

#include <radish/view/animations/unit_animation.h>

///
/// Was eine Animation ueber das sagt, was die Figur gerade tut
/// (RAD_UnitAnimationState_t.driving, firing) -- daran haengen die Folgen MOVE
/// und FIRE ihres Bildes (view/iso_map_view.c).
///

void test_animation_faehrt_nur_waehrend_einer_fahrt(void)
{
    // (0,0) -> (1,0) -> (1,1), Blick X_PLUS, ab 100 ms: faehrt 100..850,
    // dreht 850..1850, faehrt 1850..2600.
    const float xs[] = { 0.0f, 1.0f, 1.0f };
    const float ys[] = { 0.0f, 0.0f, 1.0f };
    RAD_UnitAnimation_t *animation = RAD_CreateUnitAnimation(xs, ys, 3, RAD_ISO_FACING_X_PLUS, 0.0f,
                                                             100.0f, 750.0f, 1000.0f, 500.0f);
    TEST_ASSERT_NOT_NULL(animation);

    TEST_ASSERT_FALSE(RAD_UnitAnimationStateAt(animation, 50.0f).driving);
    TEST_ASSERT_TRUE(RAD_UnitAnimationStateAt(animation, 100.0f).driving);
    TEST_ASSERT_TRUE(RAD_UnitAnimationStateAt(animation, 500.0f).driving);

    // Beim Drehen auf der Stelle faehrt sie nicht.
    const RAD_UnitAnimationState_t turning = RAD_UnitAnimationStateAt(animation, 1200.0f);
    TEST_ASSERT_TRUE(turning.turning);
    TEST_ASSERT_FALSE(turning.driving);

    TEST_ASSERT_TRUE(RAD_UnitAnimationStateAt(animation, 2000.0f).driving);
    TEST_ASSERT_FALSE(RAD_UnitAnimationStateAt(animation, 2600.0f).driving);
    TEST_ASSERT_FALSE(RAD_UnitAnimationStateAt(animation, INFINITY).driving);

    // Ein Zug feuert nicht.
    TEST_ASSERT_FALSE(RAD_UnitAnimationStateAt(animation, 500.0f).firing);

    RAD_DestroyUnitAnimation(&animation);
}

void test_animation_auf_der_stelle_faehrt_nicht(void)
{
    const float xs[] = { 2.0f };
    const float ys[] = { 3.0f };
    RAD_UnitAnimation_t *animation = RAD_CreateUnitAnimation(xs, ys, 1, RAD_ISO_FACING_Y_PLUS, 0.0f,
                                                             0.0f, 750.0f, 1000.0f, 500.0f);
    TEST_ASSERT_NOT_NULL(animation);
    TEST_ASSERT_FALSE(RAD_UnitAnimationStateAt(animation, 300.0f).driving);
    RAD_DestroyUnitAnimation(&animation);
}

void test_animation_feuert_waehrend_des_haltens(void)
{
    // Zielt ab 0 ms von 0 auf 90 Grad (500 ms je Vierteldrehung), haelt 400 ms,
    // dreht zurueck: zielt 0..500, feuert 500..900, zurueck 900..1400.
    RAD_UnitAnimation_t *animation = RAD_CreateUnitAimAnimation(0.0f, 0.0f, RAD_ISO_FACING_X_MINUS, 0.0f,
                                                                90.0f, true, 400.0f, 0.0f, 500.0f);
    TEST_ASSERT_NOT_NULL(animation);

    float fire_ms = 0.0f;
    TEST_ASSERT_TRUE(RAD_UnitAnimationFireMs(animation, &fire_ms));
    TEST_ASSERT_EQUAL_FLOAT(500.0f, fire_ms);

    TEST_ASSERT_FALSE(RAD_UnitAnimationStateAt(animation, 250.0f).firing);

    const RAD_UnitAnimationState_t firing = RAD_UnitAnimationStateAt(animation, 650.0f);
    TEST_ASSERT_TRUE(firing.firing);
    TEST_ASSERT_EQUAL_FLOAT(150.0f, firing.firing_ms);
    TEST_ASSERT_EQUAL_FLOAT(90.0f, firing.aim_angle);
    TEST_ASSERT_FALSE(firing.driving);

    TEST_ASSERT_FALSE(RAD_UnitAnimationStateAt(animation, 1000.0f).firing);
    TEST_ASSERT_FALSE(RAD_UnitAnimationStateAt(animation, INFINITY).firing);

    RAD_DestroyUnitAnimation(&animation);
}
