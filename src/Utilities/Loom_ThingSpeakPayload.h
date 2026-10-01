#pragma once

#include "Loom_WarningGuards.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Arduino.h>
LOOM_EXTERNAL_INCLUDE_END
#include <cstdio>
#include <cstring>

namespace loomThingSpeak {
////////////////////////////////////////////////////////////////////////////////////////////////////
// Keep the eight readings, then format one small piece at a time for MQTT. We never need a
// whole 1 KB text buffer, and measuring the text length never calls a sensor a second time.
class Payload : public Stream {
  public:
    static constexpr size_t MAX_FIELDS = 8;

    bool addField(int number, float value) {
        if (ready || fieldCount == MAX_FIELDS || number < 1 || number > 8) {
            return false;
        }
        fields[fieldCount++] = {number, value};
        return true;
    }

    // Timestamp storage belongs to Manager. It must remain valid until publishStream finishes.
    // Check every piece before opening the MQTT frame; malformed text must not be half-sent.
    bool prepare(const char *localTime) {
        timestamp = localTime;
        ready = false;
        totalLength = 0;
        partCount = fieldCount + (timestamp != nullptr ? 1 : 0) + 1; // Last part is status.
        for (size_t part = 0; part < partCount; ++part) {
            if (!formatPart(part)) {
                remaining = 0;
                return false;
            }
            totalLength += pieceLength;
        }
        nextPart = 0;
        pieceOffset = pieceLength = 0;
        remaining = totalLength;
        ready = true;
        return true;
    }

    size_t length() const { return totalLength; }
    int available() override { return ready ? static_cast<int>(remaining) : 0; }

    int peek() override {
        if (!ready || remaining == 0) {
            return -1;
        }
        if (pieceOffset == pieceLength) {
            if (nextPart == partCount || !formatPart(nextPart++)) {
                ready = false;
                return -1;
            }
            pieceOffset = 0;
        }
        return static_cast<unsigned char>(piece[pieceOffset]);
    }

    int read() override {
        const int value = peek();
        if (value >= 0) {
            ++pieceOffset;
            --remaining;
        }
        return value;
    }

    // Read-only adapter. Stream::readBytes() uses read(); nothing may write into this payload.
    size_t write(uint8_t) override { return 0; }

  private:
    struct Field {
        int number;
        float value;
    };
    Field fields[MAX_FIELDS]{};
    size_t fieldCount = 0;
    const char *timestamp = nullptr;
    char piece[100]{};
    size_t pieceOffset = 0, pieceLength = 0;
    size_t nextPart = 0, partCount = 0;
    size_t totalLength = 0, remaining = 0;
    bool ready = false;

    bool formatPart(size_t part) {
        int written = 0;
        if (part < fieldCount) {
            written = snprintf(piece, sizeof(piece), "field%i=%f&", fields[part].number,
                               fields[part].value);
        } else if (timestamp != nullptr && part == fieldCount) {
            written = snprintf(piece, sizeof(piece), "created_at=%s&", timestamp);
        } else {
            written = snprintf(piece, sizeof(piece), "status=MQTTPUBLISH");
        }
        if (written < 0 || static_cast<size_t>(written) >= sizeof(piece)) {
            return false;
        }
        pieceLength = static_cast<size_t>(written);
        return true;
    }
};
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomThingSpeak
