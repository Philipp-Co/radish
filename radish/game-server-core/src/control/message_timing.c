#include <radish/server/control/message_timing.h>

#include <time.h>

void RAD_MessageTimingInitialize(RAD_MessageTiming_t *timing)
{
    timing->count = 0;
    timing->total_ns = 0;
    timing->max_ns = 0;
}

uint64_t RAD_MessageTimingNow(void)
{
    struct timespec now;
    clock_gettime(CLOCK_MONOTONIC, &now);
    return (uint64_t)now.tv_sec * 1000000000ULL + (uint64_t)now.tv_nsec;
}

void RAD_MessageTimingAdd(RAD_MessageTiming_t *timing, uint64_t duration_ns)
{
    timing->count++;
    timing->total_ns += duration_ns;
    if(duration_ns > timing->max_ns)
    {
        timing->max_ns = duration_ns;
    }
}

uint64_t RAD_MessageTimingAverage(const RAD_MessageTiming_t *timing)
{
    if(timing->count == 0)
    {
        return 0;
    }
    return timing->total_ns / timing->count;
}
