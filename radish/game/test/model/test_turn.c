#include <unity.h>
#include <radish/game/model/turn/turn.h>

///
/// Der Zug liegt hinter dem privaten Suchpfad -- dass dieser Test uebersetzt,
/// ist zugleich die Probe darauf, dass die Tests ihn bekommen.
///
void test_turn_faengt_leer_an(void)
{
    RAD_Turn_t turn = RAD_CreateTurn();

    TEST_ASSERT_EQUAL_INT(0, RAD_TurnNumberOfUsers(&turn));
    TEST_ASSERT_EQUAL_UINT64(RAD_USER_NONE, RAD_TurnCurrentUser(&turn));
    TEST_ASSERT_EQUAL_INT(0, RAD_TurnActionPoints(&turn));
}
