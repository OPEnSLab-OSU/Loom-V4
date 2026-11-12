#pragma once

#include <cmath>
#include <stdio.h>

namespace bench {

// One versioned reading. Little-endian integers, hundredths for climate, and AS5311 steps
// for displacement. The presence mask keeps missing sensor values distinct from zero.
constexpr uint8_t CompactBytes = 27;
constexpr uint8_t CompactVersion = 1, CompactEmpty = 2;

inline uint32_t read32(const uint8_t *p) {
    return uint32_t(read16(p)) | (uint32_t(read16(p + 2)) << 16);
}

inline void write32(uint8_t *p, uint32_t value) {
    write16(p, static_cast<uint16_t>(value));
    write16(p + 2, static_cast<uint16_t>(value >> 16));
}

inline JsonVariantConst compactField(JsonVariantConst doc, const char *module, const char *key) {
    for (JsonObjectConst entry : doc["contents"].as<JsonArrayConst>()) {
        const char *name = entry["module"].as<const char *>();
        if (name && !strcmp(name, module)) return entry["data"][key];
    }
    return {};
}

inline bool compactNumber(JsonVariantConst field, double scale, int32_t low, int32_t high, int32_t &value) {
    if (!field.is<float>()) return false;
    const double scaled = std::round(field.as<double>() * scale);
    if (!std::isfinite(scaled) || scaled < low || scaled > high) return false;
    value = static_cast<int32_t>(scaled);
    return true;
}

inline size_t packCompact(JsonVariantConst doc, uint8_t node, uint8_t source, uint8_t *out) {
    static const char *const alignments[] = {"Green", "Yellow", "Red", "Error"};
    const bool synthetic = source == SyntheticSample;
    const char *type = doc["type"].as<const char *>();
    memset(out, 0, CompactBytes);
    out[0] = type && !strcmp(type, "empty") ? CompactEmpty : CompactVersion;
    out[1] = node;
    if (out[0] == CompactEmpty) return CompactBytes;
    write32(out + 3, compactField(doc, "Packet", "Number").as<uint32_t>());
    if (doc["sample_ms"].is<uint32_t>()) {
        out[2] |= 0x80;
        write32(out + 7, doc["sample_ms"].as<uint32_t>());
    }
    const char *climate = synthetic ? "Bench" : "SHT31";
    const JsonVariantConst values[] = {
        compactField(doc, climate, "Temperature_C"),
        compactField(doc, climate, synthetic ? "Humidity_pct" : "Humidity_%RH"),
        compactField(doc, "AS5311", "mag"), compactField(doc, "AS5311", "pos_raw"),
        compactField(doc, "AS5311", "pos_avg")
    };
    for (uint8_t i = 0; i < 5; ++i) {
        int32_t value;
        const int32_t low = i == 0 ? INT16_MIN : 0;
        const int32_t high = i == 0 ? INT16_MAX : i == 1 ? 10000 : 4095;
        if (compactNumber(values[i], i < 2 ? 100.0 : 1.0, low, high, value)) {
            out[2] |= uint8_t(1) << i;
            write16(out + 11 + 2 * i, static_cast<uint16_t>(value));
        }
    }
    const char *alignment = compactField(doc, "AS5311", "Alignment").as<const char *>();
    out[21] = 3;
    for (uint8_t i = 0; alignment && i < 4; ++i) {
        if (!strcmp(alignment, alignments[i])) { out[21] = i; out[2] |= 0x20; break; }
    }
    int32_t steps;
    if (compactNumber(compactField(doc, "displacement", "um"), 4096.0 / 2000.0, INT32_MIN, INT32_MAX, steps)) {
        out[2] |= 0x40;
        write32(out + 22, static_cast<uint32_t>(steps));
    }
    return CompactBytes;
}

inline bool unpackCompact(const uint8_t *data, size_t size, uint8_t node, uint8_t source, Manager &manager) {
    static const char *const alignments[] = {"Green", "Yellow", "Red", "Error"};
    if (size != CompactBytes || data[1] != node || data[21] > 3 || data[26] ||
        (data[0] != CompactVersion && data[0] != CompactEmpty)) return false;
    const uint8_t present = data[2];
    if ((present & 2) && read16(data + 13) > 10000) return false;
    for (uint8_t i = 0; i < 3; ++i) {
        if ((present & (uint8_t(4) << i)) && read16(data + 15 + 2 * i) > 4095) return false;
    }
    auto &doc = manager.getDocument();
    doc.clear();
    char name[12];
    snprintf(name, sizeof(name), "LoRaNode%u", static_cast<unsigned>(node));
    doc["id"]["name"] = name;
    doc["id"]["instance"] = node;
    doc["type"] = data[0] == CompactEmpty ? "empty" : "data";
    if (data[0] == CompactEmpty) return manager.isPacketValid();
    if (present & 0x80) doc["sample_ms"] = read32(data + 7);
    manager.addData("Packet", "Number", read32(data + 3));
    const bool synthetic = source == SyntheticSample;
    const char *climate = synthetic ? "Bench" : "SHT31";
    if (present & 1) manager.addData(climate, "Temperature_C", static_cast<int16_t>(read16(data + 11)) / 100.0f);
    if (present & 2) manager.addData(climate, synthetic ? "Humidity_pct" : "Humidity_%RH", read16(data + 13) / 100.0f);
    if (!synthetic) {
        const char *const fields[] = {"mag", "pos_raw", "pos_avg"};
        for (uint8_t i = 0; i < 3; ++i) {
            if (present & (uint8_t(4) << i)) manager.addData("AS5311", fields[i], read16(data + 15 + 2 * i));
        }
        if (present & 0x20) manager.addData("AS5311", "Alignment", alignments[data[21]]);
        if (present & 0x40) manager.addData("displacement", "um", static_cast<int32_t>(read32(data + 22)) * (2000.0f / 4096));
    }
    return manager.isPacketValid();
}

} // namespace bench
