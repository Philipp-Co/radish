#include <unity.h>

#include <radish/server/control/message_timing.h>

///
/// Die Zeitmessung je Nachricht (RAD_MessageTiming_t): Hoechstwert und
/// Durchschnitt seit dem Start. Gerechnet wird mit festen Dauern -- die Uhr
/// selbst wird nur darauf geprueft, dass sie nicht rueckwaerts laeuft.
///

void test_zeitmessung_ohne_nachricht_ist_alles_null(void)
{
    RAD_MessageTiming_t timing;
    RAD_MessageTimingInitialize(&timing);

    TEST_ASSERT_EQUAL_UINT64(0, timing.count);
    TEST_ASSERT_EQUAL_UINT64(0, timing.max_ns);
    TEST_ASSERT_EQUAL_UINT64(0, RAD_MessageTimingAverage(&timing));
}

void test_zeitmessung_merkt_hoechstwert_und_durchschnitt(void)
{
    RAD_MessageTiming_t timing;
    RAD_MessageTimingInitialize(&timing);

    RAD_MessageTimingAdd(&timing, 300);
    RAD_MessageTimingAdd(&timing, 900);
    RAD_MessageTimingAdd(&timing, 600);

    TEST_ASSERT_EQUAL_UINT64(3, timing.count);
    TEST_ASSERT_EQUAL_UINT64(1800, timing.total_ns);
    TEST_ASSERT_EQUAL_UINT64(900, timing.max_ns);
    TEST_ASSERT_EQUAL_UINT64(600, RAD_MessageTimingAverage(&timing));
}

void test_zeitmessung_rundet_den_durchschnitt_ab(void)
{
    RAD_MessageTiming_t timing;
    RAD_MessageTimingInitialize(&timing);

    RAD_MessageTimingAdd(&timing, 1);
    RAD_MessageTimingAdd(&timing, 2);

    TEST_ASSERT_EQUAL_UINT64(1, RAD_MessageTimingAverage(&timing));
}

void test_zeitmessung_uhr_laeuft_nicht_rueckwaerts(void)
{
    const uint64_t first = RAD_MessageTimingNow();
    const uint64_t second = RAD_MessageTimingNow();

    TEST_ASSERT_TRUE(second >= first);
}
