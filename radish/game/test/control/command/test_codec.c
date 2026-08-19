#include <unity.h>
#include <radish/game/control/command/codec.h>

///
/// Die Wire-Nummern sind eine Zusage an die Gegenseite: sie haengen nicht an der
/// Reihenfolge im enum und duerfen sich nicht verschieben. Dieser Test haelt sie
/// fest -- der Rest der Codec-Tests kommt aus dem bisherigen Pruefprogramm.
///
void test_codec_kennt_die_wire_nummern(void)
{
    TEST_ASSERT_EQUAL_UINT8(2, RAD_CommandTypeToWire(RAD_COMMAND_TYPE_MOVE_ENTITY));
    TEST_ASSERT_EQUAL_UINT8(6, RAD_CommandTypeToWire(RAD_COMMAND_TYPE_END_TURN));

    // Die reservierte 0 gehoert zu keiner Art.
    bool ok = true;
    RAD_CommandTypeFromWire(0, &ok);
    TEST_ASSERT_FALSE(ok);

    RAD_CommandType_t type = RAD_CommandTypeFromWire(2, &ok);
    TEST_ASSERT_TRUE(ok);
    TEST_ASSERT_EQUAL_INT(RAD_COMMAND_TYPE_MOVE_ENTITY, type);
}
