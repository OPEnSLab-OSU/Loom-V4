#pragma once

#include "Loom_GnssUtils.h"
#include <cmath>
#include <cstdlib>

namespace loomGnss {
// CGNSINF firmware revisions differ in their trailing satellite fields. Only the
// documented run state, fix state, UTC and coordinates are needed for a Loom Fix.
inline bool parseSim70xx(char *text, Fix &out) {
    if (!text) return false;
    char *fields[5] = {text};
    for (size_t i = 1; i < 5; ++i) {
        char *comma = std::strchr(fields[i - 1], ',');
        if (!comma) return false;
        *comma = '\0';
        fields[i] = comma + 1;
    }
    char *end = std::strchr(fields[4], ',');
    if (!end) return false;
    *end = '\0';
    if (std::strcmp(fields[0], "1") || std::strcmp(fields[1], "1")) return false;
    const char *utc = fields[2];
    const size_t length = std::strlen(utc);
    if (length < 14 || !decimalDigits(utc, 14)) return false;
    if (length > 14 && (utc[14] != '.' || length == 15 ||
                       !decimalDigits(utc + 15, length - 15))) return false;
    Fix parsed;
    parsed.year = twoDigits(utc) * 100 + twoDigits(utc + 2);
    parsed.month = twoDigits(utc + 4);
    parsed.day = twoDigits(utc + 6);
    parsed.hour = twoDigits(utc + 8);
    parsed.minute = twoDigits(utc + 10);
    parsed.second = twoDigits(utc + 12);
    if (!loomTime::validUtcFields(parsed.year, parsed.month, parsed.day,
                                  parsed.hour, parsed.minute, parsed.second)) return false;
    double *coordinates[] = {&parsed.latitude, &parsed.longitude};
    for (size_t i = 0; i < 2; ++i) {
        const char *value = fields[3 + i];
        const char *digits = *value == '-' || *value == '+' ? value + 1 : value;
        if (*digits < '0' || *digits > '9') return false;
        bool dot = false;
        for (const char *c = digits; *c; ++c) {
            if (*c == '.' && !dot) dot = true;
            else if (*c < '0' || *c > '9') return false;
        }
        char *tail = nullptr;
        *coordinates[i] = std::strtod(value, &tail);
        const double limit = i == 0 ? 90.0 : 180.0;
        if (!tail || *tail || !std::isfinite(*coordinates[i]) ||
            *coordinates[i] < -limit || *coordinates[i] > limit) return false;
    }
    out = parsed;
    return true;
}
} // namespace loomGnss
