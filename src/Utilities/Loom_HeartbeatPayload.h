#pragma once
#include "Loom_WarningGuards.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <ArduinoJson.h>
LOOM_EXTERNAL_INCLUDE_END
#include <cstddef>
#include <cstring>

namespace loomHeartbeat {
// JSON strings have an explicit length. Reject embedded NULs before a C-string topic copy
// could silently shorten a device name; validate both fields before proxy identity changes.
inline bool validIdentity(JsonObjectConst identity) {
    const JsonString name = identity["name"].as<JsonString>();
    return !name.isNull() && name.size() > 0 && name.size() < 64 &&
           std::memchr(name.c_str(), '\0', name.size()) == nullptr &&
           identity["instance"].is<int>() && identity["instance"].as<int>() >= 0;
}
inline bool validDeviceName(const char *name) {
    if (name == nullptr || name[0] == '\0') {
        return false;
    }
    for (size_t length = 0; length < 64; ++length) {
        if (name[length] == '\0') {
            return true;
        }
    }
    return false; // Same 63-character ceiling as Manager; no unbounded string scan.
}
////////////////////////////////////////////////////////////////////////////////////////////////////
// A heartbeat says who is alive. It does not contain measurements or a measurement packet
// number. The caller may add fresh time/battery/health fields after building this base packet.
// The name is borrowed: it must stay alive while the document is used (Manager's name does).
inline bool buildStatusPayload(JsonDocument &output, const char *name, int instance) {
    output.clear();
    if (!validDeviceName(name) || instance < 0) {
        return false;
    }
    output["type"] = "heartbeat";
    output.createNestedArray("contents");
    output["id"]["name"] = name;
    output["id"]["instance"] = instance;
    return !output.overflowed();
}

// Heartbeat-only radio mode also checks explicit JSON sends, so data cannot bypass the mode.
inline bool isStatusPayload(JsonObjectConst packet) {
    return packet["type"] == "heartbeat" && packet["contents"].is<JsonArrayConst>() &&
           packet["contents"].as<JsonArrayConst>().size() == 0 &&
           validIdentity(packet["id"].as<JsonObjectConst>());
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomHeartbeat
