#include "Utilities/Loom_Sim70xxGnss.h"
#include <cassert>
#include <cmath>
#include <cstring>

bool parse(const char *raw, loomGnss::Fix &fix) {
    char text[256];
    assert(std::strlen(raw) < sizeof(text));
    std::strcpy(text, raw);
    return loomGnss::parseSim70xx(text, fix);
}

int main() {
    loomGnss::Fix fix;
    assert(parse("1,1,20261118001530.000,44.5646,-123.2620,80,0,0,1,,,,,,,,,", fix));
    assert(std::fabs(fix.latitude - 44.5646) < 1e-9);
    assert(std::fabs(fix.longitude + 123.2620) < 1e-9);
    assert(fix.year == 2026 && fix.month == 11 && fix.day == 18 && fix.second == 30);
    assert(parse("1,1,20240229120000,-90,180,0", fix));
    const auto saved = fix;
    const char *invalid[] = {
        "1,0,20261118001530.000,44,-123,0",
        "0,1,20261118001530.000,44,-123,0",
        "1,1,20260229120000,44,-123,0",
        "1,1,20261118240000,44,-123,0",
        "1,1,20261118001530.,44,-123,0",
        "1,1,20261118001530,91,-123,0",
        "1,1,20261118001530,44,-181,0",
        "1,1,20261118001530,nan,-123,0",
        "1,1,20261118001530,,0,0",
        "1,1,20261118001530,44,",
        "1,1,20261118001530,44,1e2,0"
    };
    for (const char *raw : invalid) {
        assert(!parse(raw, fix));
        assert(fix.latitude == saved.latitude && fix.longitude == saved.longitude);
        assert(fix.year == saved.year && fix.day == saved.day);
    }
    assert(!loomGnss::parseSim70xx(nullptr, fix));
}
