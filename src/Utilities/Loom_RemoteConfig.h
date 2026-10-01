#pragma once

#include "Loom_WarningGuards.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <ArduinoJson.h>
LOOM_EXTERNAL_INCLUDE_END
#include "Loom_TimeUtils.h"

namespace loomRemote {
////////////////////////////////////////////////////////////////////////////////////////////////////
// Retained remote commands use clock-style component ranges. Missing components mean zero,
// but a present string, fraction, null or too-wide integer is an error, not an implicit zero.
inline bool parseSleepInterval(JsonObjectConst json, int32_t &seconds) {
    if (json.isNull()) {
        return false;
    }
    const char *fields[] = {"days", "hours", "minutes", "seconds"};
    const int32_t maxima[] = {27, 23, 59, 59};
    const int32_t scales[] = {86400, 3600, 60, 1};
    int32_t total = 0;
    for (size_t index = 0; index < 4; ++index) {
        if (!json.containsKey(fields[index])) {
            continue;
        }
        JsonVariantConst value = json[fields[index]];
        if (!value.is<int32_t>() || value.as<int32_t>() > maxima[index] ||
            !loomTime::addIntervalPart(value.as<int32_t>(), scales[index], total)) {
            return false;
        }
    }
    if (!loomTime::validRtcWakeDelay(total)) {
        return false;
    }
    seconds = total; // Preserve the caller's prior interval on every rejected command.
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomRemote
