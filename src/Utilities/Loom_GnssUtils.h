#pragma once

#include "Loom_TimeUtils.h"
#include "Loom_ATReply.h"
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace loomGnss {
////////////////////////////////////////////////////////////////////////////////////////////////////
// Coordinates are decimal degrees. The date/time belongs to this fix, always in UTC;
// it is not the time at which the sketch read the modem's cached RMC message.
struct Fix {
    double latitude = 0.0, longitude = 0.0;
    uint16_t year = 0;
    uint8_t month = 0, day = 0, hour = 0, minute = 0, second = 0;
};

inline int hexDigit(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    return -1;
}

inline bool decimalDigits(const char *text, size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (text[i] < '0' || text[i] > '9') {
            return false;
        }
    }
    return true;
}

inline uint8_t twoDigits(const char *text) {
    // Called only after digit validation: two decimal digits fit in a byte (0 through 99).
    return static_cast<uint8_t>((text[0] - '0') * 10 + text[1] - '0');
}

// NMEA uses degrees followed by minutes, not a decimal-degree number.
// Read only its specified decimal syntax: no signs, exponents, NaN or trailing junk.
inline bool coordinate(const char *text, const char *hemisphere, bool latitude, double &out) {
    const size_t degreesLength = latitude ? 2 : 3;
    const size_t length = strlen(text);
    if (length < degreesLength + 2 || !decimalDigits(text, degreesLength + 2)) {
        return false;
    }
    unsigned int degrees = 0;
    for (size_t i = 0; i < degreesLength; ++i) {
        degrees = degrees * 10 + text[i] - '0';
    }
    double minutes = twoDigits(text + degreesLength);
    if (length > degreesLength + 2) {
        if (text[degreesLength + 2] != '.' || length == degreesLength + 3) {
            return false;
        }
        double scale = 0.1;
        for (size_t i = degreesLength + 3; i < length; ++i) {
            if (text[i] < '0' || text[i] > '9') {
                return false;
            }
            minutes += (text[i] - '0') * scale;
            scale *= 0.1;
        }
    }
    const unsigned int maximum = latitude ? 90 : 180;
    if (minutes >= 60.0 || degrees > maximum || (degrees == maximum && minutes != 0.0) ||
        strlen(hemisphere) != 1) {
        return false;
    }
    const char direction = hemisphere[0];
    if (latitude ? (direction != 'N' && direction != 'S')
                 : (direction != 'E' && direction != 'W')) {
        return false;
    }
    out = degrees + minutes / 60.0;
    if (direction == 'S' || direction == 'W') {
        out = -out;
    }
    return true;
}

// Parse in place after verifying the checksum. Failure leaves the caller's Fix unchanged.
// A bounded response line owns this text; splitting fields does not allocate another copy.
inline bool parseRmc(char *sentence, Fix &out) {
    if (sentence == nullptr || sentence[0] != '$') {
        return false;
    }
    char *checksum = strchr(sentence, '*');
    if (checksum == nullptr || strlen(checksum + 1) != 2) {
        return false;
    }
    const int high = hexDigit(checksum[1]), low = hexDigit(checksum[2]);
    if (high < 0 || low < 0) {
        return false;
    }
    uint8_t calculated = 0;
    for (char *p = sentence + 1; p < checksum; ++p) {
        calculated ^= static_cast<uint8_t>(*p);
    }
    if (calculated != static_cast<uint8_t>((high << 4) | low)) {
        return false;
    }
    *checksum = '\0';

    char *fields[14]{};
    size_t count = 1;
    fields[0] = sentence + 1;
    for (char *p = sentence + 1; *p != '\0'; ++p) {
        if (*p == ',') {
            if (count == 14) {
                return false;
            }
            *p = '\0';
            fields[count++] = p + 1;
        }
    }
    if (count < 12 || strlen(fields[0]) != 5 || strcmp(fields[0] + 2, "RMC") != 0 ||
        fields[0][0] < 'A' || fields[0][0] > 'Z' || fields[0][1] < 'A' || fields[0][1] > 'Z' ||
        strcmp(fields[2], "A") != 0) {
        return false;
    }
    // R510M8S emits autonomous/differential fixes. Do not present invalid or estimated mode
    // positions as GNSS fixes. Older RMC layouts have no mode field.
    if (count >= 13 && strcmp(fields[12], "A") != 0 && strcmp(fields[12], "D") != 0) {
        return false;
    }
    if (count == 14 && strcmp(fields[13], "V") == 0) {
        return false;
    }

    const size_t timeLength = strlen(fields[1]);
    if (timeLength < 6 || !decimalDigits(fields[1], 6) || strlen(fields[9]) != 6 ||
        !decimalDigits(fields[9], 6)) {
        return false;
    }
    if (timeLength > 6 &&
        (fields[1][6] != '.' || timeLength == 7 || !decimalDigits(fields[1] + 7, timeLength - 7))) {
        return false;
    }
    Fix candidate;
    candidate.hour = twoDigits(fields[1]);
    candidate.minute = twoDigits(fields[1] + 2);
    candidate.second = twoDigits(fields[1] + 4);
    candidate.day = twoDigits(fields[9]);
    candidate.month = twoDigits(fields[9] + 2);
    candidate.year = static_cast<uint16_t>(2000u + twoDigits(fields[9] + 4));
    if (!loomTime::validUtcFields(candidate.year, candidate.month, candidate.day, candidate.hour,
                                  candidate.minute, candidate.second) ||
        !coordinate(fields[3], fields[4], true, candidate.latitude) ||
        !coordinate(fields[5], fields[6], false, candidate.longitude)) {
        return false;
    }
    out = candidate;
    return true;
}

// Read one complete AT reply. Ignore unrelated URCs and echo, require an exact final OK,
// and commit a fix only after that OK. Both elapsed time and total bytes are bounded even
// with continuous noise. Oversized lines are discarded, never parsed as truncated sentences.
template <typename Stream, typename Clock, typename Idle>
bool readReply(Stream &stream, Fix *fix, Clock now, Idle idle, uint32_t timeoutMs) {
    char line[160]{};
    size_t length = 0;
    bool overflow = false, gotFix = false;
    Fix candidate;
    const uint32_t start = now();
    size_t received = 0;
    while (static_cast<uint32_t>(now() - start) < timeoutMs && received < 2048) {
        if (stream.available() <= 0) {
            idle();
            continue;
        }
        const int next = stream.read();
        if (next < 0) {
            idle();
            continue;
        }
        ++received;
        const char c = static_cast<char>(next);
        if (c == '\r') {
            continue;
        }
        if (c != '\n') {
            if (c == '\0' || length == sizeof(line) - 1) {
                overflow = true;
            }
            if (!overflow) {
                line[length++] = c;
            }
            continue;
        }
        line[length] = '\0';
        if (!overflow) {
            const loomAt::Reply reply = loomAt::classifyLine(line);
            if (reply == loomAt::Reply::Accepted) {
                if (fix != nullptr && !gotFix) {
                    return false;
                }
                if (fix != nullptr) {
                    *fix = candidate;
                }
                return true;
            }
            if (reply == loomAt::Reply::Rejected) {
                return false;
            }
            if (fix != nullptr && strncmp(line, "+UGRMC:", 7) == 0) {
                char *message = line + 7;
                while (*message == ' ') {
                    ++message;
                }
                // R5 read replies include the enabled storage state before the RMC sentence.
                gotFix = strncmp(message, "1,", 2) == 0 && parseRmc(message + 2, candidate);
            }
        }
        length = 0;
        overflow = false;
    }
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomGnss
