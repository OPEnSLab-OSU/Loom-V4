#include "../../src/Utilities/Loom_FileTimestampCache.h"
#include <assert.h>

int main() {
    loomTime::FileTimestampCache cache;
    uint32_t utc = 7;
    assert(!cache.get(0, utc) && utc == 7);
    cache.update(1790834400UL, 100);
    assert(cache.get(1099, utc) && utc == 1790834400UL);
    assert(!cache.get(1100, utc)); // Refresh after one second; never extrapolate the RTC.
    cache.unavailable(1100);
    assert(cache.fresh(1101) && !cache.get(1101, utc)); // Failed reads are throttled too.
    assert(!cache.fresh(2100));
    cache.update(1790834400UL, 2100);
    cache.invalidate(); // Standby may last hours while millis() barely advances.
    assert(!cache.fresh(2101) && !cache.get(2101, utc));
    cache.update(1790841600UL, 2101);
    assert(cache.get(2101, utc) && utc == 1790841600UL);
    cache.update(1790841540UL, 2102); // Network correction replaces old time immediately.
    assert(cache.get(2102, utc) && utc == 1790841540UL);
    cache.update(1790841540UL, UINT32_MAX - 500);
    assert(cache.get(498, utc)); // Unsigned elapsed time survives millis() rollover.
    assert(!cache.get(499, utc));
    return 0;
}
