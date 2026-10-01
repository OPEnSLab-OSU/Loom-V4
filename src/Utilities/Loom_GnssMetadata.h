#pragma once
#include "Loom_WarningGuards.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <ArduinoJson.h>
LOOM_EXTERNAL_INCLUDE_END
#include "Loom_GnssUtils.h"
#include <cmath>
#include <cstdio>

namespace loomGnss {
////////////////////////////////////////////////////////////////////////////////////////////////////
// Explicit metadata opt-in. A cached receiver reply may be old; reject future/stale fixes.
// This never queries a modem or changes RTC time. The JSON document owns the UTC text.
inline bool isFreshFix(const Fix &fix, uint32_t nowUtc, uint32_t maximumAgeSeconds = 300) {
    if (!std::isfinite(fix.latitude) || !std::isfinite(fix.longitude) || fix.latitude < -90.0 ||
        fix.latitude > 90.0 || fix.longitude < -180.0 || fix.longitude > 180.0 ||
        !loomTime::validUtcFields(fix.year, fix.month, fix.day, fix.hour, fix.minute, fix.second)) {
        return false;
    }
    const uint32_t fixUtc =
        loomTime::unixSeconds(fix.year, fix.month, fix.day, fix.hour, fix.minute, fix.second);
    return fixUtc <= nowUtc && nowUtc - fixUtc <= maximumAgeSeconds;
}

// Shared writer: callers below have already checked coordinate/date bounds and freshness.
inline bool writeValidatedLocation(JsonObject location, const Fix &fix) {
    if (location.isNull()) {
        return false;
    }
    char utc[21]; // Four-digit year + fixed UTC fields + Z + terminator.
    const int written =
        snprintf(utc, sizeof(utc), "%04u-%02u-%02uT%02u:%02u:%02uZ",
                 static_cast<unsigned int>(fix.year), static_cast<unsigned int>(fix.month),
                 static_cast<unsigned int>(fix.day), static_cast<unsigned int>(fix.hour),
                 static_cast<unsigned int>(fix.minute), static_cast<unsigned int>(fix.second));
    if (written != 20) {
        return false;
    }
    location["LocationMethod"] = "GPS"; // Same method label as the reviewed location branch.
    location["Latitude"] = fix.latitude;
    location["Longitude"] = fix.longitude;
    location["time_utc"] = utc;
    // Caller must also reject an overflowed outer document at the publish boundary.
    return !location.isNull() && location.size() == 4 && !location["time_utc"].isNull();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
inline bool addMetadata(JsonObject metadata, const Fix &fix, uint32_t nowUtc,
                        uint32_t maximumAgeSeconds = 300) {
    // Reject before replacing existing metadata, so a bad reply cannot overwrite a good fix.
    return !metadata.isNull() && isFreshFix(fix, nowUtc, maximumAgeSeconds) &&
           writeValidatedLocation(metadata.createNestedObject("location"), fix);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
// Call after EVERY package(), starting with the first SD row. Keep the same four columns even
// without a receiver/fix: otherwise disappearing location fields rotate the CSV header.
// Pass Manager::get_data_object("Location"). This also includes location in ordinary uploads.
// False means unavailable or out of JSON space; check Manager::isPacketValid() before saving.
inline bool addCsvLocation(JsonObject columns, const Fix *fix, uint32_t nowUtc,
                           uint32_t maximumAgeSeconds = 300) {
    if (columns.isNull()) {
        return false;
    }
    if (fix != nullptr && isFreshFix(*fix, nowUtc, maximumAgeSeconds)) {
        return writeValidatedLocation(columns, *fix);
    }
    columns["LocationMethod"] = nullptr;
    columns["Latitude"] = nullptr;
    columns["Longitude"] = nullptr;
    columns["time_utc"] = nullptr;
    return false; // Blank cells say unavailable; never reuse expired coordinates silently.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomGnss
