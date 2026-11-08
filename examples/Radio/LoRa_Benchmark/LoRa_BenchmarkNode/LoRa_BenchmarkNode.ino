#include <Loom_Manager.h>
#include "BenchRadio.h"

using namespace bench;

constexpr uint8_t NodeId = 1; // Flash a different ID (1 through 16) onto each node.
constexpr bool SendJson = false;
constexpr bool SyntheticData = true;
constexpr uint16_t PaddingBytes = 0; // Increase to sweep the size of the same sample.

static_assert(NodeId >= 1 && NodeId <= MaxNodes, "Use a node ID from 1 through 16");
static_assert(PaddingBytes < MAX_JSON_SIZE, "Padding exceeds the Loom packet budget");

Manager manager("LoRaNode", NodeId);
// Declare real Loom sensor modules here. Set SyntheticData to false for a sensor-only run.
RH_RF95 radio(RFM95_CS, RFM95_INT);
uint8_t payload[MaxChunk * MaxFragments + 1]; // Allow a JSON terminator at full capacity.
char padding[PaddingBytes + 1] = {};
bool ready = false;
uint32_t sent = 0, skipped = 0, lastReport = 0;

void setup() {
    manager.beginSerial(false);
    // Enable the sensor rail here before manager.initialize() if your sensors require it.
    manager.initialize();
    memset(padding, 'x', PaddingBytes);
    ready = beginRadio(radio, NodeId);
    if (!ready) {
        Serial.println(F("RFM95 init failed"));
    }
}

void loop() {
    if (!ready) {
        delay(1000);
        return;
    }

    uint8_t frame[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t length = sizeof(frame);
    if (!radio.recv(frame, &length)) {
        return;
    }
    const uint32_t beaconEnd = millis();
    if (length != BeaconBytes || radio.headerFrom() != HubAddress ||
        radio.headerTo() != RH_BROADCAST_ADDRESS || frame[0] != NetworkId || frame[1] != Beacon) {
        return;
    }
    const uint16_t round = read16(frame + 2);
    const Schedule schedule{frame[4], frame[5], frame[6], read16(frame + 7)};
    if (!schedule.valid() || NodeId > schedule.nodes) {
        return;
    }

    manager.measure();
    manager.package();
    if (SyntheticData) {
        JsonObject data = manager.get_data_object("Bench");
        data["Temperature_C"] = 19.0f + NodeId * 0.25f;
        data["Humidity_pct"] = 50 + NodeId;
        data["Counter"] = round;
        if (PaddingBytes > 0) {
            data["Padding"] = static_cast<const char *>(padding);
        }
    }

    const JsonDocument &sample = manager.getDocument();
    const size_t jsonBytes = measureJson(sample);
    const size_t packedBytes = measureMsgPack(sample);
    const size_t bytes = SendJson ? jsonBytes : packedBytes;
    const size_t capacity = size_t(schedule.chunk) * schedule.fragments;
    const uint32_t slot = slotStartMs(schedule, NodeId);
    const uint32_t start = slot + GuardMs;
    const uint32_t end = slot + slotMs(schedule) - GuardMs / 2;
    const __FlashStringHelper *error = nullptr;

    if (!manager.isPacketValid()) {
        error = F("invalid packet");
    } else if (bytes > capacity || jsonBytes >= MAX_JSON_SIZE) {
        error = F("oversize");
    } else {
        const size_t written = SendJson ? serializeJson(sample, payload, bytes + 1)
                                        : serializeMsgPack(sample, payload, bytes);
        if (written != bytes) {
            error = F("encode failed");
        } else {
            while (millis() - beaconEnd < start) {
                delay(1);
            }
            const uint8_t fragments = static_cast<uint8_t>((bytes + schedule.chunk - 1) / schedule.chunk);
            for (uint8_t i = 0; i < fragments; ++i) {
                const size_t offset = size_t(i) * schedule.chunk;
                const uint8_t count = static_cast<uint8_t>(min(size_t(schedule.chunk), bytes - offset));
                const uint8_t frameBytes = HeaderBytes + count;
                if (millis() - beaconEnd + airtimeMs(frameBytes) + FrameMarginMs > end) {
                    error = F("slot budget");
                    break;
                }
                frame[0] = NetworkId;
                frame[1] = SendJson ? Json : MsgPack;
                write16(frame + 2, round);
                frame[4] = i;
                frame[5] = fragments;
                memcpy(frame + HeaderBytes, payload + offset, count);
                if (!sendFrame(radio, frame, frameBytes)) {
                    error = F("radio TX failed");
                    break;
                }
            }
        }
    }

    const bool complete = error == nullptr;
    if (complete) {
        ++sent;
    } else {
        ++skipped;
    }
    if (!complete || sent == 1 || millis() - lastReport >= 10000) {
        lastReport = millis();
        Serial.print(F("node=")); Serial.print(NodeId);
        Serial.print(F(" round=")); Serial.print(round);
        Serial.print(F(" json_B=")); Serial.print(jsonBytes);
        Serial.print(F(" msgpack_B=")); Serial.print(packedBytes);
        Serial.print(F(" sent=")); Serial.print(sent);
        Serial.print(F(" skipped=")); Serial.print(skipped);
        Serial.print(F(" result=")); Serial.println(complete ? F("sent") : error);
    }
}
