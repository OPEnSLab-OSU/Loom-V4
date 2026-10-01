#pragma once

#include <math.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

namespace loomSDI12 {
// SDI-12 M! measurements return at most nine values. Our supported sensors need at most eight.
constexpr size_t MAX_VALUES = 8;
enum class Model { Unknown, GS3, TER11, TER12, TER21, TER54 };

inline Model identifyModel(const char *info) {
    // I! has fixed-width fields: address(1), protocol(2), vendor(8), model(6).
    // Reading only the model avoids mistaking a model number in the serial number for a sensor.
    if (info == nullptr || strlen(info) < 20) {
        return Model::Unknown;
    }
    const char *model = info + 11;
    if (strncmp(model, "GS3   ", 6) == 0) {
        return Model::GS3;
    }
    if (strncmp(model, "TER11 ", 6) == 0) {
        return Model::TER11;
    }
    if (strncmp(model, "TER12 ", 6) == 0) {
        return Model::TER12;
    }
    if (strncmp(model, "TER21 ", 6) == 0) {
        return Model::TER21;
    }
    if (strncmp(model, "TER54 ", 6) == 0) {
        return Model::TER54;
    }
    return Model::Unknown;
}

inline uint8_t valueCount(Model model) {
    switch (model) {
    case Model::GS3:
    case Model::TER12:
        return 3;
    case Model::TER11:
    case Model::TER21:
        return 2;
    case Model::TER54:
        return 8;
    default:
        return 0;
    }
}

inline bool parseMeasurementReply(const char *reply, char address, uint16_t &waitSeconds,
                                  uint8_t &values) {
    // M! replies are exactly atttn after CR/LF removal. Reject noise, other addresses and C!
    // replies.
    if (reply == nullptr || strlen(reply) != 5 || reply[0] != address) {
        return false;
    }
    for (size_t i = 1; i < 5; ++i) {
        if (reply[i] < '0' || reply[i] > '9') {
            return false;
        }
    }
    waitSeconds =
        static_cast<uint16_t>((reply[1] - '0') * 100 + (reply[2] - '0') * 10 + (reply[3] - '0'));
    values = static_cast<uint8_t>(reply[4] - '0');
    return values > 0 && values <= MAX_VALUES;
}

inline bool appendDataReply(const char *reply, char address, float *values, size_t capacity,
                            size_t &count) {
    if (reply == nullptr || values == nullptr || reply[0] != address || reply[1] == '\0' ||
        count > capacity) {
        return false;
    }
    const char *cursor = reply + 1;
    while (*cursor != '\0') {
        // Each value starts with its own sign; negative temperatures have no '+' separator.
        if (count == capacity || (*cursor != '+' && *cursor != '-')) {
            return false;
        }
        const char *start = cursor++;
        bool digitSeen = false;
        bool pointSeen = false;
        while ((*cursor >= '0' && *cursor <= '9') || *cursor == '.') {
            if (*cursor == '.') {
                if (pointSeen) {
                    return false;
                }
                pointSeen = true;
            } else {
                digitSeen = true;
            }
            ++cursor;
        }
        if (!digitSeen) {
            return false;
        }
        char *end = nullptr;
        const float parsed = strtof(start, &end);
        if (end != cursor || !isfinite(parsed)) {
            return false;
        }
        values[count++] = parsed;
    }
    return true;
}
} // namespace loomSDI12
