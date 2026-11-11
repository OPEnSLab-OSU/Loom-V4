#pragma once
/**
 * Compact LoRa format for the dendrometer: a whole batch of readings in one message, and the hub's schedule reply.
 * Batch: header [FORMAT][node][count][first packet u16][first time u32][RSSI i8][flags: 1 = more messages follow], then
 *        per reading
 *        [packet offset][seconds offset u16][temp x100 i16][RH x100 u16][(mV - 3000) / 10]
 *        [5 bytes: mag 12 bits | pos_raw 12 | pos_avg 12 | alignment 2 | button 1][displacement steps i16]
 * Schedule (hub -> node, after a node's last message): [SCHEDULE][hub time u32][next send time u32][cycle seconds u32],
 *           unix times in UTC; the node then sends every cycle from the next send time on
 * Multi-byte values are little-endian. Both start with a byte below 0x80, so Loom_LoRa hands them over as raw messages.
 */
#include <ArduinoJson.h>
#include <OPEnS_RTC.h>

namespace DendrometerPacket {

const uint8_t FORMAT = 1;     // First byte of a batch
const uint8_t HEADER_SIZE = 11;
const uint8_t READING_SIZE = 15;
const uint8_t MAX_READINGS = 16;  // 11 + 16 * 15 = 251, the largest RadioHead RF95 payload
const uint8_t SCHEDULE = 3;       // First byte of the hub's schedule reply
const uint8_t SCHEDULE_SIZE = 13;

/* Builds a batch message on the node */
class Writer {
public:
    /**
     * Add one logged reading. Returns false if it doesn't fit (16 already, or a reboot restarted the packet numbers):
     * send the message, clear() it and add the reading again. A reading without a time is skipped (returns true).
     */
    bool add(JsonDocument& reading, uint8_t node);
    const uint8_t* data() const { return buf; }
    uint8_t length() const { return count ? HEADER_SIZE + count * READING_SIZE : 0; }
    void clear() { count = 0; }
    /* Tell the hub another message of this send comes right after this one, so it waits before replying */
    void setMoreFollows(bool more) { buf[10] = more; }

private:
    uint8_t buf[HEADER_SIZE + MAX_READINGS * READING_SIZE];
    uint8_t count = 0;
    uint16_t firstPacket = 0;
    uint32_t firstTime = 0;
};

/* Number of readings in a received batch, or 0 if it isn't a valid one */
uint8_t readingCount(const uint8_t* msg, uint8_t len);
inline uint8_t nodeNumber(const uint8_t* msg) { return msg[1]; }
inline bool moreFollows(const uint8_t* msg) { return msg[10] & 1; }

/* Unix time (UTC) and packet number of reading `index` in a valid batch */
uint32_t readingTime(const uint8_t* msg, uint8_t index);
uint32_t readingPacket(const uint8_t* msg, uint8_t index);

/**
 * Rebuild reading `index` of a batch as the JSON document the node logged. `name` is the device name for the id (the
 * message carries only the node number); `toLocal` converts UTC to local time for time_local (null leaves it in UTC).
 */
void unpack(const uint8_t* msg, uint8_t index, JsonDocument& doc, const char* name, DateTime (*toLocal)(DateTime));

/* The hub's reply: its current time (the node sets its clock to it), when the node sends next and how often */
void makeSchedule(uint8_t out[SCHEDULE_SIZE], uint32_t hubTime, uint32_t nextSend, uint32_t cycle);
bool readSchedule(const uint8_t* msg, uint8_t len, uint32_t& hubTime, uint32_t& nextSend, uint32_t& cycle);  // false if not one

}  // namespace DendrometerPacket
