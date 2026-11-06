#pragma once
/**
 * DRAFT. Automatic, lossless bit-packing of a BatchSD batch (one Loom JSON reading per line) for uploading as one MQTT
 * message. Nothing to configure: the batch itself decides, field by field, how few bits each value needs.
 *
 * For every field (e.g. contents/SHT31/Temperature_C) across the readings in the batch:
 *   numbers     the fewest decimals that rebuild every value exactly (as the float the sensor produced), then only
 *               the offset from the batch minimum, in just enough bits (21.66..22.31 C -> 2 decimals, 7 bits)
 *   timestamps  time_utc as seconds after the batch minimum; time_local as its offset from time_utc
 *   text        a dictionary of the distinct values, then an index per reading
 *   true/false  1 bit
 *   anything else (mixed types, null, a value no decimal count can rebuild) is still sent exactly, just less compactly
 * Readings that aren't the standard Loom layout are sent as JSON text. Consecutive readings with the same fields form a
 * block; a message holds one block per layout.
 *
 * Message (multi-byte numbers are LEB128 varints, signed ones zigzagged; bits are packed LSB first):
 *   'P' 0x01 varint(blocks) block...
 *   block: varint(rows) varint(fields) field-header... bits(rows x fields)
 *   field-header: text(path) u8(kind) then
 *     NUMBER: u8(decimals) svarint(base) u8(bits)       value = (base + n) / 10^decimals
 *     TEXT, JSON: varint(count) text... u8(bits)          value = dictionary[n]  (JSON entries are JSON text)
 *     BOOLEAN: -                                          1 bit
 *     TIME_UTC: svarint(base) u8(bits)                    unix time = base + n
 *     TIME_OFFSET: svarint(base) u8(bits)                 local time = this row's time_utc + base + n
 *     DOUBLE: -                                           64 bits, the raw IEEE double
 *   text: varint(length) bytes
 * The MQTT bridge (packed_bridge.py) decodes "<database>/<device>/packed" topics.
 */
#include <Arduino.h>
#include <ArduinoJson.h>
#include <OPEnS_RTC.h>
#include <Adafruit_SleepyDog.h>
#include <math.h>
#include <string.h>
#include <vector>

namespace PackedBatch {

const uint8_t MAGIC = 'P';
const uint8_t VERSION = 1;
const uint8_t MAX_DECIMALS = 8;
const uint8_t MAX_BITS = 32;  // a wider range is sent as raw doubles
enum Kind : uint8_t { NUMBER = 0, TEXT = 1, BOOLEAN = 2, TIME_UTC = 3, TIME_OFFSET = 4, JSON = 5, DOUBLE = 6 };

/* Counts bytes instead of sending them, to learn a message's size before streaming it */
class Counter : public Print {
public:
    size_t count = 0;
    size_t write(uint8_t) override { count++; return 1; }
    size_t write(const uint8_t*, size_t n) override { count += n; return n; }
};

/* Writes values LSB first into a byte stream */
class BitWriter {
public:
    explicit BitWriter(Print& out) : out(out) {}
    void write(uint64_t value, uint8_t bits) {
        for (uint8_t i = 0; i < bits; i++) {
            if ((value >> i) & 1)
                current |= 1 << used;
            if (++used == 8)
                flush();
        }
    }
    size_t finish() {
        if (used)
            flush();
        return written;
    }

private:
    void flush() {
        out.write(current);
        written++;
        current = used = 0;
    }
    Print& out;
    uint8_t current = 0, used = 0;
    size_t written = 0;
};

inline size_t writeVarint(Print& out, uint64_t v) {
    size_t n = 0;
    do {
        uint8_t b = v & 0x7F;
        v >>= 7;
        out.write(v ? b | 0x80 : b);
        n++;
    } while (v);
    return n;
}
inline size_t writeSigned(Print& out, int64_t v) { return writeVarint(out, ((uint64_t)v << 1) ^ (uint64_t)(v >> 63)); }
inline size_t writeText(Print& out, const char* s) {
    size_t len = strlen(s);
    return writeVarint(out, len) + out.write((const uint8_t*)s, len);
}

inline uint8_t bitsFor(uint64_t span) {
    uint8_t bits = 0;
    while (span >> bits)
        bits++;
    return bits;
}
inline int64_t pow10i(uint8_t e) {
    int64_t p = 1;
    while (e--)
        p *= 10;
    return p;
}
inline bool endsWith(const String& s, const char* tail) {
    size_t n = strlen(tail);
    return s.length() >= n && strcmp(s.c_str() + s.length() - n, tail) == 0;
}

/* A Loom_Hypnos timestamp ("2026-11-04T02:05:07Z" UTC, no Z local) that its own text can be rebuilt from */
inline bool hypnosTime(const char* text, bool utc, uint32_t& unixTime) {
    int year, month, day, hour, minute, second;
    if (text == nullptr || sscanf(text, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
        return false;
    DateTime t(year, month, day, hour, minute, second);
    char rebuilt[21];
    snprintf(rebuilt, sizeof(rebuilt), utc ? "%04u-%02u-%02uT%02u:%02u:%02uZ" : "%04u-%02u-%02uT%02u:%02u:%02u",
             t.year(), t.month(), t.day(), t.hour(), t.minute(), t.second());
    unixTime = t.unixtime();
    return strcmp(rebuilt, text) == 0;
}

/* Fewest decimals (0..MAX_DECIMALS) that rebuild this float exactly, or MAX_DECIMALS + 1 if none do */
inline uint8_t decimalsFor(float f) {
    for (uint8_t e = 0; e <= MAX_DECIMALS; e++) {
        double scale = pow10i(e);
        if ((float)(llround((double)f * scale) / scale) == f)
            return e;
    }
    return MAX_DECIMALS + 1;
}

/* Standard Loom layout: type, id, contents of {module, data}, timestamp, with scalar values; anything else goes as JSON text */
inline bool standardLayout(JsonDocument& r) {
    for (JsonPair kv : r.as<JsonObject>()) {
        const char* key = kv.key().c_str();
        JsonVariant v = kv.value();
        if (!strcmp(key, "contents")) {
            if (!v.is<JsonArray>())
                return false;
            for (JsonVariant m : v.as<JsonArray>()) {
                if (!m.is<JsonObject>() || m.size() != 2 || !m["module"].is<const char*>() || !m["data"].is<JsonObject>())
                    return false;
                if (strlen(m["module"]) > 40)
                    return false;
                for (JsonPair d : m["data"].as<JsonObject>())
                    if (d.value().is<JsonObject>() || d.value().is<JsonArray>() || strlen(d.key().c_str()) > 40)
                        return false;
            }
        } else if (v.is<JsonObject>()) {
            for (JsonPair d : v.as<JsonObject>())
                if (d.value().is<JsonObject>() || d.value().is<JsonArray>() || strlen(d.key().c_str()) > 40)
                    return false;
        } else if (v.is<JsonArray>()) {
            return false;
        }
    }
    return true;
}

/* Call f(path, value) for every field of a reading, in the reading's own order */
template <typename F>
void forEachField(JsonDocument& r, F f) {
    if (!standardLayout(r)) {
        f("json", r.as<JsonVariant>());
        return;
    }
    char path[100];
    for (JsonPair kv : r.as<JsonObject>()) {
        const char* key = kv.key().c_str();
        if (!strcmp(key, "contents")) {
            for (JsonObject m : kv.value().as<JsonArray>())
                for (JsonPair d : m["data"].as<JsonObject>()) {
                    snprintf(path, sizeof(path), "contents/%s/%s", m["module"].as<const char*>(), d.key().c_str());
                    f(path, d.value());
                }
        } else if (kv.value().is<JsonObject>()) {
            for (JsonPair d : kv.value().as<JsonObject>()) {
                snprintf(path, sizeof(path), "%s/%s", key, d.key().c_str());
                f(path, d.value());
            }
        } else {
            f(key, kv.value());
        }
    }
}

/* Read and parse readings starting at `start` (a file position); blank or corrupt lines are skipped and not counted */
template <typename BatchFile, typename F>
void forEachReading(BatchFile& batch, uint32_t start, uint32_t count, F f) {
    batch.seek(start);
    uint32_t done = 0;
    while (done < count && batch.available()) {
        Watchdog.reset();  // a long batch over LTE can take longer than a sketch-enabled watchdog period
        uint32_t position = batch.position();
        String line = batch.readStringUntil('\n');
        line.trim();
        if (line.length() == 0)
            continue;
        DynamicJsonDocument reading(line.length() * 2 + 512);
        if (deserializeJson(reading, line) != DeserializationError::Ok)
            continue;
        reading.shrinkToFit();  // the hub has little RAM to spare during uploads
        f(reading, position);
        done++;
    }
}

struct Field {
    String path;
    bool number = false, text = false, boolean = false, other = false, utc = false, offset = false;
    uint8_t decimals = 0;
    double minValue = INFINITY, maxValue = -INFINITY;
    std::vector<String> dictionary;
    Kind kind = JSON;
    uint8_t bits = 0;
    int64_t base = 0;

    void note(double v) {
        if (v < minValue) minValue = v;
        if (v > maxValue) maxValue = v;
    }
    uint32_t index(const String& s) {
        for (uint32_t i = 0; i < dictionary.size(); i++)
            if (dictionary[i] == s)
                return i;
        dictionary.push_back(s);
        return dictionary.size() - 1;
    }
};

inline String jsonText(JsonVariant v) {
    String s;
    serializeJson(v, s);
    return s;
}

/* The value a NUMBER field stores for v, before subtracting the base */
inline int64_t scaled(JsonVariant v, uint8_t decimals) {
    if (v.is<long long>())
        return v.as<long long>() * pow10i(decimals);
    return llround((double)v.as<float>() * pow10i(decimals));
}

/* First pass over a block: what each field holds */
inline void observe(std::vector<Field>& fields, JsonDocument& reading) {
    size_t i = 0;
    bool haveUtc = false;
    uint32_t rowUtc = 0;
    forEachField(reading, [&](const char* path, JsonVariant v) {
        Field& f = fields[i++];
        uint32_t t;
        if (v.is<bool>()) {
            f.boolean = true;
        } else if (v.is<const char*>()) {
            if (endsWith(f.path, "/time_utc") && hypnosTime(v, true, t)) {
                f.utc = true;
                f.note(t);
                haveUtc = true;
                rowUtc = t;
            } else if (endsWith(f.path, "/time_local") && haveUtc && hypnosTime(v, false, t)) {
                f.offset = true;
                f.note((double)t - rowUtc);
            } else {
                f.text = true;
                f.index(v.as<const char*>());
            }
        } else if (v.is<long long>()) {
            f.number = true;
            f.note(v.as<long long>());
        } else if (v.is<double>()) {
            f.number = true;
            float x = v.as<float>();
            uint8_t e = decimalsFor(x);
            if (e > f.decimals)
                f.decimals = e;
            f.note(x);
        } else {
            f.other = true;
        }
    });
}

/* Choose each field's kind and size from what the first pass saw */
inline bool decide(Field& f) {
    int kinds = f.number + f.text + f.boolean + f.other + f.utc + f.offset;
    if (kinds != 1 || f.other) {
        f.kind = JSON;  // mixed or unusual values; the dictionary is filled by an extra pass
        f.dictionary.clear();
        return true;
    }
    if (f.boolean) {
        f.kind = BOOLEAN;
        f.bits = 1;
    } else if (f.text) {
        f.kind = TEXT;
        f.bits = bitsFor(f.dictionary.size() > 1 ? f.dictionary.size() - 1 : 0);
    } else if (f.utc || f.offset) {
        f.kind = f.utc ? TIME_UTC : TIME_OFFSET;
        f.base = (int64_t)f.minValue;
        f.bits = bitsFor((uint64_t)((int64_t)f.maxValue - f.base));
    } else {
        uint8_t bits = 64;
        if (f.decimals <= MAX_DECIMALS) {
            double scale = pow10i(f.decimals);
            int64_t lo = llround(f.minValue * scale), hi = llround(f.maxValue * scale);
            bits = bitsFor((uint64_t)(hi - lo));
            f.base = lo;
        }
        if (bits <= MAX_BITS) {
            f.kind = NUMBER;
            f.bits = bits;
        } else {
            f.kind = DOUBLE;
            f.bits = 64;
        }
    }
    return false;
}

inline size_t writeHeader(Print& out, Field& f) {
    size_t n = writeText(out, f.path.c_str()) + out.write((uint8_t)f.kind);
    switch (f.kind) {
        case NUMBER:
            n += out.write(f.decimals) + writeSigned(out, f.base) + out.write(f.bits);
            break;
        case TEXT:
        case JSON:
            n += writeVarint(out, f.dictionary.size());
            for (String& s : f.dictionary)
                n += writeText(out, s.c_str());
            n += out.write(f.bits);
            break;
        case TIME_UTC:
        case TIME_OFFSET:
            n += writeSigned(out, f.base) + out.write(f.bits);
            break;
        default:
            break;
    }
    return n;
}

/* Second pass: one reading's values */
inline void writeRow(std::vector<Field>& fields, JsonDocument& reading, BitWriter& bits) {
    size_t i = 0;
    uint32_t rowUtc = 0;
    forEachField(reading, [&](const char* path, JsonVariant v) {
        Field& f = fields[i++];
        uint32_t t = 0;
        switch (f.kind) {
            case BOOLEAN:
                bits.write(v.as<bool>(), 1);
                break;
            case TEXT:
                bits.write(f.index(v.as<const char*>()), f.bits);
                break;
            case JSON:
                bits.write(f.index(jsonText(v)), f.bits);
                break;
            case TIME_UTC:
                hypnosTime(v, true, t);
                rowUtc = t;
                bits.write((uint64_t)((int64_t)t - f.base), f.bits);
                break;
            case TIME_OFFSET:
                hypnosTime(v, false, t);
                bits.write((uint64_t)((int64_t)t - rowUtc - f.base), f.bits);
                break;
            case NUMBER:
                bits.write((uint64_t)(scaled(v, f.decimals) - f.base), f.bits);
                break;
            case DOUBLE: {
                double d = v.as<double>();
                uint64_t raw;
                memcpy(&raw, &d, 8);
                bits.write(raw, 64);
                break;
            }
        }
    });
}

/**
 * Encode a batch file to `out` and return how many bytes were written. Encoding is deterministic, so encode once to a
 * Counter for the message size, then again to the MQTT stream.
 */
template <typename BatchFile>
size_t encode(BatchFile& batch, Print& out) {
    // Blocks: runs of consecutive readings with the same fields
    struct Block {
        uint32_t start, rows;
    };
    std::vector<Block> blocks;
    String lastLayout;
    forEachReading(batch, 0, UINT32_MAX, [&](JsonDocument& reading, uint32_t position) {
        String layout;
        forEachField(reading, [&](const char* path, JsonVariant) {
            layout += path;
            layout += '\n';
        });
        if (blocks.empty() || layout != lastLayout) {
            blocks.push_back({position, 0});
            lastLayout = layout;
        }
        blocks.back().rows++;
    });
    lastLayout = String();

    size_t written = out.write(MAGIC) + out.write(VERSION) + writeVarint(out, blocks.size());
    for (Block& block : blocks) {
        std::vector<Field> fields;
        bool needJson = false;
        forEachReading(batch, block.start, block.rows, [&](JsonDocument& reading, uint32_t) {
            if (fields.empty())
                forEachField(reading, [&](const char* path, JsonVariant) {
                    fields.emplace_back();
                    fields.back().path = path;
                });
            observe(fields, reading);
        });
        for (Field& f : fields)
            needJson |= decide(f);
        if (needJson)  // dictionaries of JSON text for fields with mixed values
            forEachReading(batch, block.start, block.rows, [&](JsonDocument& reading, uint32_t) {
                size_t i = 0;
                forEachField(reading, [&](const char*, JsonVariant v) {
                    Field& f = fields[i++];
                    if (f.kind == JSON)
                        f.index(jsonText(v));
                });
            });
        for (Field& f : fields)
            if (f.kind == JSON)
                f.bits = bitsFor(f.dictionary.size() > 1 ? f.dictionary.size() - 1 : 0);

        written += writeVarint(out, block.rows) + writeVarint(out, fields.size());
        for (Field& f : fields)
            written += writeHeader(out, f);
        BitWriter bits(out);
        forEachReading(batch, block.start, block.rows, [&](JsonDocument& reading, uint32_t) { writeRow(fields, reading, bits); });
        written += bits.finish();
    }
    return written;
}

}  // namespace PackedBatch
