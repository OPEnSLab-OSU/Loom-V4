#include <cassert>
#include <ArduinoJson.h>

class Manager {
  public:
    StaticJsonDocument<2048> &getDocument() { return document; }
    bool isPacketValid() const { return !document.overflowed(); }

    template <typename Value>
    void addData(const char *module, const char *key, Value value) {
        JsonArray contents = document["contents"];
        if (contents.isNull()) contents = document.createNestedArray("contents");
        for (JsonObject entry : contents) {
            if (!strcmp(entry["module"].as<const char *>(), module)) {
                entry["data"][key] = value;
                return;
            }
        }
        JsonObject entry = contents.createNestedObject();
        entry["module"] = module;
        entry["data"][key] = value;
    }
  private:
    StaticJsonDocument<2048> document;
};

namespace bench {
constexpr uint8_t SensorSample = 0, SyntheticSample = 1;
uint16_t read16(const uint8_t *p) { return uint16_t(p[0]) | (uint16_t(p[1]) << 8); }
void write16(uint8_t *p, uint16_t value) {
    p[0] = uint8_t(value);
    p[1] = uint8_t(value >> 8);
}
}

#include "Radio/Loom_LoRa/Benchmark/BenchCompact.h"

int main() {
    using namespace bench;
    Manager input, output;
    auto &doc = input.getDocument();
    doc["type"] = "data";
    doc["sample_ms"] = 0;
    input.addData("Packet", "Number", 1234567);
    input.addData("SHT31", "Temperature_C", -3.125);
    input.addData("SHT31", "Humidity_%RH", 0);
    input.addData("AS5311", "mag", 4095);
    input.addData("AS5311", "pos_raw", 5000); // Invalid reading; pos_avg is absent.
    input.addData("AS5311", "Alignment", "Yellow");
    input.addData("displacement", "um", -2000);
    uint8_t data[CompactBytes];
    assert(packCompact(doc.as<JsonVariantConst>(), 20, SensorSample, data) == sizeof(data));
    assert(data[2] == 0xe7);
    assert(!unpackCompact(data, sizeof(data) - 1, 20, SensorSample, output));
    assert(!unpackCompact(data, sizeof(data), 19, SensorSample, output));
    assert(unpackCompact(data, sizeof(data), 20, SensorSample, output));
    const auto decoded = output.getDocument().as<JsonVariantConst>();
    assert(!strcmp(decoded["id"]["name"].as<const char *>(), "LoRaNode20"));
    assert(decoded["sample_ms"].is<uint32_t>() && decoded["sample_ms"].as<uint32_t>() == 0);
    assert(compactField(decoded, "Packet", "Number").as<uint32_t>() == 1234567);
    assert(std::abs(compactField(decoded, "SHT31", "Temperature_C").as<double>() + 3.13) < 0.001);
    assert(compactField(decoded, "SHT31", "Humidity_%RH").is<float>());
    assert(compactField(decoded, "SHT31", "Humidity_%RH").as<float>() == 0);
    assert(compactField(decoded, "AS5311", "mag").as<uint16_t>() == 4095);
    assert(compactField(decoded, "AS5311", "pos_raw").isNull());
    assert(compactField(decoded, "AS5311", "pos_avg").isNull());
    assert(!strcmp(compactField(decoded, "AS5311", "Alignment").as<const char *>(), "Yellow"));
    assert(compactField(decoded, "displacement", "um").as<float>() == -2000);
    write16(data + 13, 10001);
    assert(!unpackCompact(data, sizeof(data), 20, SensorSample, output));

    doc.clear();
    doc["type"] = "empty";
    assert(packCompact(doc.as<JsonVariantConst>(), 1, SensorSample, data) == sizeof(data));
    assert(unpackCompact(data, sizeof(data), 1, SensorSample, output));
    assert(!strcmp(output.getDocument()["type"].as<const char *>(), "empty"));
    assert(output.getDocument()["contents"].isNull());
}
