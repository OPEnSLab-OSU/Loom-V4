#pragma once

#include "Loom_WarningGuards.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include <ArduinoJson.h>
LOOM_EXTERNAL_INCLUDE_END

namespace loomLoRa {
// Five top-level entries, two ID entries and two optional timestamp entries. The empty contents
// array needs no element slots. This counts JSON storage, not the transmitted MessagePack bytes.
constexpr size_t FRAGMENT_HEADER_CAPACITY = JSON_OBJECT_SIZE(5) + 2 * JSON_OBJECT_SIZE(2);

////////////////////////////////////////////////////////////////////////////////////////////////////
// A fragmented transmission starts with this small envelope; sensor contents follow separately.
// Strings are borrowed from packet, so packet must stay alive through serialization/transmission.
// Keep the field insertion order: existing receivers use this same header and padded wire format.
inline bool buildFragmentHeader(JsonDocument &header, JsonObject packet) {
    header.clear();
    header["type"] = packet["type"].as<const char *>();
    header["numPackets"] = packet["contents"].size();

    JsonObject id = header.createNestedObject("id");
    id["name"] = packet["id"]["name"].as<const char *>();
    id["instance"] = packet["id"]["instance"].as<int>();
    header.createNestedArray("contents");

    if (!packet["timestamp"].isNull()) {
        JsonObject timestamp = header.createNestedObject("timestamp");
        timestamp["time_utc"] = packet["timestamp"]["time_utc"].as<const char *>();
        timestamp["time_local"] = packet["timestamp"]["time_local"].as<const char *>();
    }
    return !header.overflowed();
}
////////////////////////////////////////////////////////////////////////////////////////////////////
} // namespace loomLoRa
