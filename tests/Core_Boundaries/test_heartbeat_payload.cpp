#include <cassert>
#include <cstring>
#include "Utilities/Loom_HeartbeatPayload.h"

int main() {
    StaticJsonDocument<512> packet;
    assert(loomHeartbeat::buildStatusPayload(packet, "node", 7));
    assert(loomHeartbeat::isStatusPayload(packet.as<JsonObjectConst>()));
    assert(!packet.containsKey("Packet") && !packet.containsKey("timestamp"));
    assert(packet["contents"].as<JsonArray>().size() == 0);
    packet["battery_voltage"] = 4.2f;
    packet["id"]["instance"] = -1;
    assert(!loomHeartbeat::isStatusPayload(packet.as<JsonObjectConst>()));
    assert(loomHeartbeat::buildStatusPayload(packet, "node", 7));
    const char embeddedName[] = {'n', 'o', 'd', 'e', '\0', 'x'};
    packet["id"]["name"] = JsonString(embeddedName, sizeof(embeddedName));
    assert(!loomHeartbeat::isStatusPayload(packet.as<JsonObjectConst>()));
    assert(loomHeartbeat::buildStatusPayload(packet, "node", 7));
    assert(loomHeartbeat::isStatusPayload(packet.as<JsonObjectConst>()));
    packet["contents"].as<JsonArray>().createNestedObject()["measurement"] = 1;
    assert(!loomHeartbeat::isStatusPayload(packet.as<JsonObjectConst>()));
    assert(loomHeartbeat::buildStatusPayload(packet, "node", 7));
    packet["type"] = "data";
    assert(!loomHeartbeat::isStatusPayload(packet.as<JsonObjectConst>()));
    assert(!loomHeartbeat::buildStatusPayload(packet, nullptr, 7) && packet.size() == 0);
    assert(!loomHeartbeat::buildStatusPayload(packet, "", 7));
    assert(!loomHeartbeat::buildStatusPayload(packet, "node", -1));
    char longName[65];
    std::memset(longName, 'a', sizeof(longName));
    longName[64] = '\0';
    assert(!loomHeartbeat::buildStatusPayload(packet, longName, 1));
    longName[63] = '\0';
    assert(loomHeartbeat::buildStatusPayload(packet, longName, 1));
    StaticJsonDocument<8> tiny;
    assert(!loomHeartbeat::buildStatusPayload(tiny, "node", 7) && tiny.overflowed());
    // The timestamp writer's mutable buffer is copied, so reusing it cannot change UTC text.
    char utc[21] = "2026-09-30T12:00:00Z";
    packet["timestamp"]["time_utc"] = utc;
    std::strcpy(utc, "2026-09-30T05:00:00 ");
    packet["timestamp"]["time_local"] = utc;
    assert(std::strcmp(packet["timestamp"]["time_utc"].as<const char *>(),
                       "2026-09-30T12:00:00Z") == 0);
}
