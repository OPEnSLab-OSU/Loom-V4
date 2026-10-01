#include <cassert>
#include <string>
#include "Utilities/Loom_LoRaPacketHeader.h"

int main() {
    DynamicJsonDocument packet(4096);
    assert(!deserializeJson(
        packet, "{\"type\":\"data\",\"id\":{\"name\":\"node\",\"instance\":7},\"contents\":[{},{}],"
                "\"timestamp\":{\"time_utc\":\"UTC\",\"time_local\":\"local\"}}"));
    StaticJsonDocument<loomLoRa::FRAGMENT_HEADER_CAPACITY> header;
    assert(loomLoRa::buildFragmentHeader(header, packet.as<JsonObject>()));
    const char *expected =
        "{\"type\":\"data\",\"numPackets\":2,\"id\":{\"name\":\"node\",\"instance\":7},"
        "\"contents\":[],\"timestamp\":{\"time_utc\":\"UTC\",\"time_local\":\"local\"}}";
    std::string json;
    serializeJson(header, json);
    assert(json == expected);
    DynamicJsonDocument reference(1024);
    assert(!deserializeJson(reference, expected));
    std::string actualWire, expectedWire;
    serializeMsgPack(header, actualWire);
    serializeMsgPack(reference, expectedWire);
    assert(actualWire == expectedWire);
    assert(header.memoryUsage() == loomLoRa::FRAGMENT_HEADER_CAPACITY);
    assert(header["id"]["name"].as<const char *>() == packet["id"]["name"].as<const char *>());

    // Borrowed strings use no extra JSON slots, including a full-size device name.
    packet["id"]["name"] = std::string(63, 'n');
    assert(loomLoRa::buildFragmentHeader(header, packet.as<JsonObject>()));
    assert(!header.overflowed() && header["id"]["name"].as<JsonString>().size() == 63);

    // Reusing a document for an untimed packet must remove the previous timestamp.
    packet.remove("timestamp");
    assert(loomLoRa::buildFragmentHeader(header, packet.as<JsonObject>()));
    assert(!header.containsKey("timestamp") && header["contents"].size() == 0);
    StaticJsonDocument<JSON_OBJECT_SIZE(1)> tooSmall;
    assert(!loomLoRa::buildFragmentHeader(tooSmall, packet.as<JsonObject>()));
}
