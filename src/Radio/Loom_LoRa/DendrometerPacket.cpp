#include "DendrometerPacket.h"
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace DendrometerPacket {

static const char* const ALIGNMENT[] = {"Green", "Yellow", "Red", "Error"};
static const float STEP_UM = 2000.0f / 4096;  // AS5311 resolution: displacement is always a whole number of steps

/* Value of data[key] for the given module in a Loom packet, or null if the module isn't there */
static JsonVariant field(JsonDocument& doc, const char* module, const char* key) {
    for (JsonObject m : doc["contents"].as<JsonArray>())
        if (strcmp(m["module"] | "", module) == 0)
            return m["data"][key].as<JsonVariant>();
    return JsonVariant();
}

/* Parse Loom's time_utc ("2026-11-03T20:51:06Z") into Unix time */
static bool parseTime(const char* text, uint32_t& unixTime) {
    int year, month, day, hour, minute, second;
    if (text == nullptr || sscanf(text, "%d-%d-%dT%d:%d:%d", &year, &month, &day, &hour, &minute, &second) != 6)
        return false;
    unixTime = DateTime(year, month, day, hour, minute, second).unixtime();
    return true;
}

static long clampLong(long value, long low, long high) { return value < low ? low : value > high ? high : value; }

static uint8_t batteryByte(JsonDocument& reading) {
    return clampLong(lround((field(reading, "Analog", "Vbat_MV").as<float>() - 3000) / 10), 0, 255);
}

static int8_t rssiByte(JsonDocument& reading) { return clampLong(field(reading, "LoRa", "RSSI").as<int>(), -128, 127); }

bool Writer::add(JsonDocument& reading, uint8_t node) {
    uint32_t time;
    if (!parseTime(reading["timestamp"]["time_utc"], time))
        return true;
    uint16_t packet = field(reading, "Packet", "Number").as<uint16_t>();

    if (count == MAX_READINGS)
        return false;
    if (count == 0) {
        firstPacket = packet;
        firstTime = time;
        int8_t rssi = rssiByte(reading);
        buf[0] = FORMAT;
        buf[1] = node;
        memcpy(buf + 3, &firstPacket, 2);
        memcpy(buf + 5, &firstTime, 4);
        memcpy(buf + 9, &rssi, 1);
        buf[10] = 0;
    } else if (packet < firstPacket || packet - firstPacket > 255 || time < firstTime || time - firstTime > 65535) {
        return false;
    }

    uint16_t secondsSinceFirst = time - firstTime;
    int16_t temperature = clampLong(lround(field(reading, "SHT31", "Temperature_C").as<float>() * 100), -32768, 32767);
    uint16_t humidity = clampLong(lround(field(reading, "SHT31", "Humidity_%RH").as<float>() * 100), 0, 65535);

    const char* alignment = field(reading, "AS5311", "Alignment") | "Error";
    uint64_t alignmentCode = 3;
    for (uint8_t i = 0; i < 4; i++)
        if (strcmp(alignment, ALIGNMENT[i]) == 0)
            alignmentCode = i;
    uint64_t magnet = (uint64_t)clampLong(field(reading, "AS5311", "mag").as<long>(), 0, 4095)
                    | (uint64_t)clampLong(field(reading, "AS5311", "pos_raw").as<long>(), 0, 4095) << 12
                    | (uint64_t)clampLong(field(reading, "AS5311", "pos_avg").as<long>(), 0, 4095) << 24
                    | alignmentCode << 36
                    | (uint64_t)field(reading, "Button", "Pressed?").as<bool>() << 38;
    int16_t steps = clampLong(lround(field(reading, "displacement", "um").as<float>() / STEP_UM), -32768, 32767);

    uint8_t* p = buf + HEADER_SIZE + count * READING_SIZE;
    p[0] = packet - firstPacket;
    memcpy(p + 1, &secondsSinceFirst, 2);
    memcpy(p + 3, &temperature, 2);
    memcpy(p + 5, &humidity, 2);
    p[7] = batteryByte(reading);
    memcpy(p + 8, &magnet, 5);
    memcpy(p + 13, &steps, 2);

    buf[2] = ++count;
    return true;
}

uint8_t readingCount(const uint8_t* msg, uint8_t len) {
    if (len < HEADER_SIZE || msg[0] != FORMAT)
        return 0;
    uint8_t count = msg[2];
    return (count > 0 && count <= MAX_READINGS && len == HEADER_SIZE + count * READING_SIZE) ? count : 0;
}

uint32_t readingTime(const uint8_t* msg, uint8_t index) {
    uint32_t firstTime;
    uint16_t secondsSinceFirst;
    memcpy(&firstTime, msg + 5, 4);
    memcpy(&secondsSinceFirst, msg + HEADER_SIZE + index * READING_SIZE + 1, 2);
    return firstTime + secondsSinceFirst;
}

uint32_t readingPacket(const uint8_t* msg, uint8_t index) {
    uint16_t firstPacket;
    memcpy(&firstPacket, msg + 3, 2);
    return firstPacket + msg[HEADER_SIZE + index * READING_SIZE];
}

void unpack(const uint8_t* msg, uint8_t index, JsonDocument& doc, const char* name, DateTime (*toLocal)(DateTime)) {
    const uint8_t* p = msg + HEADER_SIZE + index * READING_SIZE;
    uint16_t humidity;
    int16_t temperature, steps;
    uint64_t magnet = 0;
    memcpy(&temperature, p + 3, 2);
    memcpy(&humidity, p + 5, 2);
    memcpy(&magnet, p + 8, 5);
    memcpy(&steps, p + 13, 2);

    float tempC = temperature / 100.0f;
    float rh = humidity / 100.0f;
    float batteryMV = 3000 + p[7] * 10;

    doc.clear();
    doc["type"] = "data";
    doc["id"]["name"] = name;
    doc["id"]["instance"] = nodeNumber(msg);
    JsonArray contents = doc.createNestedArray("contents");
    auto module = [&](const char* moduleName) {
        JsonObject m = contents.createNestedObject();
        m["module"] = moduleName;
        return m.createNestedObject("data");
    };
    module("Packet")["Number"] = readingPacket(msg, index);
    JsonObject analog = module("Analog");
    analog["Vbat"] = batteryMV / 1000;
    analog["Vbat_MV"] = batteryMV;
    JsonObject sht = module("SHT31");
    sht["Temperature_C"] = tempC;
    sht["Humidity_%RH"] = rh;
    sht["VPD"] = (float)(0.61078 * pow(2.71828, (17.2694 * tempC) / (tempC + 237.3)) * (1 - rh / 100));  // node's measureVPD()
    module("LoRa")["RSSI"] = (int8_t)msg[9];
    JsonObject as5311 = module("AS5311");
    as5311["Alignment"] = ALIGNMENT[(magnet >> 36) & 3];
    as5311["mag"] = (uint16_t)(magnet & 0xFFF);
    as5311["pos_raw"] = (uint16_t)((magnet >> 12) & 0xFFF);
    as5311["pos_avg"] = (uint16_t)((magnet >> 24) & 0xFFF);
    module("displacement")["um"] = steps * STEP_UM;
    module("Button")["Pressed?"] = (bool)((magnet >> 38) & 1);

    // Same text format as Loom_Hypnos::dateTime_toString on the node; char arrays are copied into the document
    DateTime utc(readingTime(msg, index));
    DateTime local = toLocal ? toLocal(utc) : utc;
    char text[24];
    snprintf(text, sizeof(text), "%04u-%02u-%02uT%02u:%02u:%02uZ", utc.year(), utc.month(), utc.day(), utc.hour(), utc.minute(), utc.second());
    doc["timestamp"]["time_utc"] = text;
    snprintf(text, sizeof(text), "%04u-%02u-%02uT%02u:%02u:%02u", local.year(), local.month(), local.day(), local.hour(), local.minute(), local.second());
    doc["timestamp"]["time_local"] = text;
}

void makeSchedule(uint8_t out[SCHEDULE_SIZE], uint32_t hubTime, uint32_t nextSend, uint32_t cycle) {
    out[0] = SCHEDULE;
    memcpy(out + 1, &hubTime, 4);
    memcpy(out + 5, &nextSend, 4);
    memcpy(out + 9, &cycle, 4);
}

bool readSchedule(const uint8_t* msg, uint8_t len, uint32_t& hubTime, uint32_t& nextSend, uint32_t& cycle) {
    if (len != SCHEDULE_SIZE || msg[0] != SCHEDULE)
        return false;
    memcpy(&hubTime, msg + 1, 4);
    memcpy(&nextSend, msg + 5, 4);
    memcpy(&cycle, msg + 9, 4);
    return true;
}

}  // namespace DendrometerPacket
