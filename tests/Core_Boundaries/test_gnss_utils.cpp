#include "Utilities/Loom_GnssUtils.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

std::string sentence(const std::string &body) {
    unsigned char checksum = 0;
    for (char c : body) {
        checksum ^= static_cast<unsigned char>(c);
    }
    char ending[4];
    snprintf(ending, sizeof(ending), "*%02X", checksum);
    return "$" + body + ending;
}

bool parse(const std::string &body, loomGnss::Fix &fix) {
    std::string text = sentence(body);
    std::vector<char> writable(text.begin(), text.end());
    writable.push_back('\0');
    return loomGnss::parseRmc(writable.data(), fix);
}

struct Stream {
    std::string text;
    size_t position = 0;
    bool continuousNoise = false;
    int available() { return continuousNoise || position < text.size(); }
    int read() {
        if (continuousNoise) {
            ++position;
            return 'X';
        }
        return position < text.size() ? static_cast<unsigned char>(text[position++]) : -1;
    }
};

int main() {
    const std::string valid = "GNRMC,123456.25,A,4542.84409,N,01344.46705,E,0.082,,300926,,,A";
    loomGnss::Fix fix;
    assert(parse(valid, fix));
    assert(std::fabs(fix.latitude - 45.7140681666667) < 1e-10);
    assert(std::fabs(fix.longitude - 13.7411175) < 1e-10);
    assert(fix.year == 2026 && fix.month == 9 && fix.day == 30);
    assert(fix.hour == 12 && fix.minute == 34 && fix.second == 56);
    assert(parse("GPRMC,000000,A,9000.000,S,18000.000,W,0,0,290224,,,D", fix));
    assert(fix.latitude == -90.0 && fix.longitude == -180.0 && fix.day == 29);
    assert(parse("GLRMC,000000,A,0000,N,00000,E,0,0,010100,,", fix));
    assert(fix.latitude == 0 && fix.longitude == 0 && fix.year == 2000);
    fix.latitude = 12.0;
    for (const std::string &body : {"GNRMC,123456,V,4542.84409,N,01344.46705,E,0,0,300926,,,N",
                                    "GNRMC,123456,A,4560.0,N,01344.0,E,0,0,300926,,,A",
                                    "GNRMC,123456,A,9000.01,N,01344.0,E,0,0,300926,,,A",
                                    "GNRMC,123456,A,4542.0,N,18000.01,E,0,0,300926,,,A",
                                    "GNRMC,123456,A,45e2.0,N,01344.0,E,0,0,300926,,,A",
                                    "GNRMC,123456,A,4542.0,X,01344.0,E,0,0,300926,,,A",
                                    "GNRMC,123456,A,4542.0,N,01344.0,E,0,0,290225,,,A",
                                    "GNRMC,240000,A,4542.0,N,01344.0,E,0,0,300926,,,A",
                                    "GNRMC,123460,A,4542.0,N,01344.0,E,0,0,300926,,,A",
                                    "GNRMC,123456.,A,4542.0,N,01344.0,E,0,0,300926,,,A",
                                    "GNRMC,123456,A,4542.0,N,01344.0,E,0,0,300926,,,E",
                                    "GNRMC,123456,A,4542.0,N,01344.0,E,0,0,300926,,,A,V",
                                    "GNGGA,123456,A,4542.0,N,01344.0,E,0,0,300926,,,A",
                                    "GNRMC,123456,A,4542.0,N,01344.0,E,0,0,300926,,,A,S,extra"}) {
        assert(!parse(body, fix) && fix.latitude == 12.0);
    }
    assert(!loomGnss::parseRmc(nullptr, fix));
    std::string damaged = sentence(valid);
    damaged[2] = 'P';
    std::vector<char> text(damaged.begin(), damaged.end());
    text.push_back('\0');
    assert(!loomGnss::parseRmc(text.data(), fix) && fix.latitude == 12.0);

    uint32_t ticks = UINT32_MAX - 5;
    auto now = [&]() { return ticks; };
    auto idle = [&]() { ++ticks; };
    Stream good{"AT+UGRMC?\r\n+CEREG: 1\r\n+UGRMC: 1," + sentence(valid) + "\r\nOK\r\n"};
    assert(loomGnss::readReply(good, &fix, now, idle, 11000) && fix.year == 2026);
    fix.latitude = 12.0;
    for (const std::string &reply : {std::string("OK\r\n"), std::string("+UGRMC: 0,NULL\r\nOK\r\n"),
                                     "+UGRMC: 1," + sentence(valid) + "\r\nERROR\r\n",
                                     "+UGRMC: 1," + sentence(valid) + "\r\nNOK\r\n",
                                     "+UGRMC: 1," + sentence(valid) + "\r\n+CME ERROR: 515\r\n"}) {
        Stream bad{reply};
        assert(!loomGnss::readReply(bad, &fix, now, idle, 15) && fix.latitude == 12.0);
    }
    Stream oversized{std::string(160, 'X') + "+UGRMC: 1," + sentence(valid) + "\r\nOK\r\n"};
    assert(!loomGnss::readReply(oversized, &fix, now, idle, 15));
    Stream commands{"AT+UGPS=1,0\r\n+UUGIND: 0,3\r\nOK\r\n"};
    assert(loomGnss::readReply(commands, nullptr, now, idle, 15));
    Stream noise;
    noise.continuousNoise = true;
    assert(!loomGnss::readReply(noise, &fix, now, idle, 11000) && noise.position == 2048);
    ticks = UINT32_MAX - 5;
    Stream empty;
    assert(!loomGnss::readReply(empty, &fix, now, idle, 15) && ticks == 9);
}
