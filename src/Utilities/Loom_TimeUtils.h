#pragma once

#include <stdint.h>
#include <limits.h>
#include <string.h>

// Small integer-only checks shared by clock/configuration boundaries. No heap or RTC driver.
namespace loomTime {
// DS3231 Alarm 1 cannot match a month/year. Keep a one-shot delay shorter than the shortest
// month so the first day-of-month match really is the requested future wake.
constexpr int32_t MAX_RTC_WAKE_DELAY_SECONDS = 27L * 24 * 60 * 60;
inline bool validRtcWakeDelay(int32_t seconds) {
    return seconds > 0 && seconds <= MAX_RTC_WAKE_DELAY_SECONDS;
}

inline bool validUtcFields(int year, int month, int day, int hour, int minute, int second) {
    // Hypnos' DS3231 stores years 2000 through 2099. Reject before any narrowing conversion.
    if (year < 2000 || year > 2099 || month < 1 || month > 12 || day < 1 || hour < 0 || hour > 23 ||
        minute < 0 || minute > 59 || second < 0 || second > 59) {
        return false;
    }
    static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    const bool leapYear = year % 4 == 0 && (year % 100 != 0 || year % 400 == 0);
    const int lastDay = monthDays[month - 1] + (month == 2 && leapYear ? 1 : 0);
    return day <= lastDay;
}

inline bool addIntervalPart(int32_t value, int32_t secondsPerUnit, int32_t &totalSeconds) {
    if (value < 0 || secondsPerUnit <= 0 || totalSeconds < 0 ||
        value > (INT32_MAX - totalSeconds) / secondsPerUnit) {
        return false;
    }
    totalSeconds += value * secondsPerUnit;
    return true;
}

// Integer conversion avoids timezone-dependent libc time. Zero means invalid input;
// valid DS3231 years are 2000..2099, so zero cannot be confused with a supported date.
inline uint32_t unixSeconds(int year, int month, int day, int hour, int minute, int second) {
    if (!validUtcFields(year, month, day, hour, minute, second)) {
        return 0;
    }
    uint32_t days = 10957; // 1970-01-01 through 2000-01-01.
    for (int y = 2000; y < year; ++y) {
        days += y % 4 == 0 ? 366 : 365; // Supported range excludes the non-leap year 2100.
    }
    static const uint8_t monthDays[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    for (int m = 1; m < month; ++m) {
        days += monthDays[m - 1] + (m == 2 && year % 4 == 0 ? 1 : 0);
    }
    return (days + static_cast<uint32_t>(day - 1)) * 86400UL +
           static_cast<uint32_t>(hour) * 3600UL + static_cast<uint32_t>(minute) * 60UL +
           static_cast<uint32_t>(second);
}

// Parse the exact UTC packet timestamp, without locale, libc timezone or another RTC read.
inline bool packetUtcSeconds(const char *text, uint32_t &seconds) {
    if (!text || strlen(text) != 20 || text[4] != '-' || text[7] != '-' || text[10] != 'T' ||
        text[13] != ':' || text[16] != ':' || text[19] != 'Z') {
        return false;
    }
    const uint8_t offsets[] = {0, 5, 8, 11, 14, 17};
    int fields[6] = {};
    for (uint8_t field = 0; field < 6; ++field) {
        const uint8_t digits = field == 0 ? 4 : 2;
        for (uint8_t digit = 0; digit < digits; ++digit) {
            const char value = text[offsets[field] + digit];
            if (value < '0' || value > '9') { return false; }
            fields[field] = fields[field] * 10 + value - '0';
        }
    }
    const uint32_t parsed = unixSeconds(fields[0], fields[1], fields[2], fields[3], fields[4], fields[5]);
    if (parsed == 0) { return false; }
    seconds = parsed;
    return true;
}

// Keep a previous due wake as an optional anchor when an interval changes.
inline uint32_t sampleIntervalAnchor(uint32_t now, uint32_t previousTarget,
                                     uint32_t previousClock, uint32_t previousInterval,
                                     uint32_t interval, bool keepLastWakeAnchor) {
    if (now < previousClock) {
        return 0; // A backwards clock correction invalidates the old phase.
    }
    if (interval != previousInterval &&
        !(keepLastWakeAnchor && previousTarget != 0 && previousTarget <= now)) {
        return 0; // Changed interval: only an already-due wake is a reusable anchor.
    }
    return previousTarget;
}

// Advance an absolute UTC grid and skip missed slots; retain a still-future target on retries.
inline bool nextSampleTime(uint32_t now, uint32_t interval, uint32_t previousTarget,
                           uint32_t &target) {
    if (interval == 0) {
        return false;
    }
    if (previousTarget == 0) {
        if (interval > UINT32_MAX - now) {
            return false;
        }
        target = now + interval;
        return true;
    }
    if (previousTarget > now) {
        target = previousTarget;
        return true;
    }
    const uint32_t elapsed = now - previousTarget;
    const uint32_t remainder = elapsed % interval;
    const uint32_t advance = interval - remainder;
    if (advance > UINT32_MAX - now) {
        return false;
    }
    target = now + advance;
    return true;
}
} // namespace loomTime
