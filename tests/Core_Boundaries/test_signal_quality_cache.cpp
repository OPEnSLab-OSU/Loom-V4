#include <Utilities/Loom_SignalQualityCache.h>
#include <cassert>
#include <cstdio>
int main() {
    using loomSignal::QualityCache;
    static_assert(sizeof(QualityCache) == 1, "RSSI cache must remain one byte");
    QualityCache cache;
    assert(!cache.hasValue());
    assert(!cache.observe(99) && !cache.hasValue());
    assert(cache.observe(0) && cache.hasValue() && cache.value() == 0);
    assert(!cache.observe(-1) && cache.value() == 0);
    assert(cache.observe(31) && cache.value() == 31);
    assert(!cache.observe(99) && cache.value() == 31);
    assert(!cache.observe(32) && cache.value() == 31);
    assert(cache.observe(17) && cache.value() == 17);
    QualityCache nextBoot;
    assert(!nextBoot.hasValue());
    std::puts("PASS one-byte RSSI cache: unknown boot, valid zero/max, failures retain last reading, fresh boot clears");
}
