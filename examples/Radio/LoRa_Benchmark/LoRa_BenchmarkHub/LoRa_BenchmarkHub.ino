#include <Loom_Manager.h>
#include "BenchRadio.h"

#ifndef BENCH_ENABLE_LTE
#define BENCH_ENABLE_LTE 0
#endif
#if BENCH_ENABLE_LTE
#include "arduino_secrets.h"
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#endif

using namespace bench;

// Nodes, bytes per fragment, fragments per slot, sample preparation time (ms).
constexpr Schedule Plan{2, MaxChunk, 1, 120};
constexpr bool PrintFirstSample = true;
constexpr uint32_t ReportMs = 10000;
constexpr uint32_t IdleAfterRoundMs = 0;
constexpr size_t SlotBytes = size_t(Plan.chunk) * Plan.fragments;
static_assert(Plan.valid(), "Invalid bench schedule");
static_assert(Plan.nodes * SlotBytes <= 8192, "Reduce the receive buffer for SAMD21 RAM");

Manager manager("LoRaHub", 0);
RH_RF95 radio(RFM95_CS, RFM95_INT);

#if BENCH_ENABLE_LTE
Loom_LTE lte(manager, BENCH_APN, BENCH_APN_USER, BENCH_APN_PASS,
             A5, BENCH_LTE_BOARD, -1, BENCH_LTE_MODEM);
Loom_MongoDB mqtt(manager, lte, BENCH_BROKER, BENCH_PORT, BENCH_DATABASE,
                 BENCH_MQTT_USER, BENCH_MQTT_PASS);
#endif

struct Received {
    uint16_t bytes;
    uint8_t format, next, fragments;
    int16_t rssi, snr;
};

struct NodeStats {
    uint32_t good, bytes, acked, partial, invalid;
    int16_t rssi, snr;
};

uint8_t payloads[Plan.nodes][SlotBytes];
Received received[Plan.nodes];
NodeStats stats[Plan.nodes] = {};
uint32_t rounds = 0, radioMs = 0, lteMs = 0, reportStart = 0, lastReport = 0;
uint32_t beaconFailures = 0;
bool ready = false, printed = false;

void setup() {
    manager.beginSerial(false);
#if BENCH_ENABLE_LTE
    if (BENCH_LTE_SUPPLY_PIN >= 0) {
        digitalWrite(BENCH_LTE_SUPPLY_PIN, HIGH);
        pinMode(BENCH_LTE_SUPPLY_PIN, OUTPUT);
        delay(1000);
    }
#endif
    manager.initialize();
    ready = beginRadio(radio, HubAddress);
    if (!ready) {
        Serial.println(F("RFM95 init failed"));
    }
    reportStart = millis();
}

void receiveRound(uint16_t round, uint32_t beaconEnd) {
    const uint32_t roundEnd = slotStartMs(Plan, Plan.nodes) + slotMs(Plan);
    while (millis() - beaconEnd < roundEnd) {
        uint8_t frame[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t length = sizeof(frame);
        if (!radio.recv(frame, &length)) {
            continue;
        }
        const uint32_t elapsed = millis() - beaconEnd;
        const uint8_t node = radio.headerFrom();
        if (node < 1 || node > Plan.nodes || radio.headerTo() != HubAddress ||
            length <= HeaderBytes || length > HeaderBytes + Plan.chunk ||
            frame[0] != NetworkId || (frame[1] != MsgPack && frame[1] != Json) ||
            read16(frame + 2) != round) {
            continue;
        }
        const uint32_t start = slotStartMs(Plan, node);
        if (elapsed < start || elapsed >= start + slotMs(Plan)) {
            continue;
        }
        const uint8_t index = frame[4], fragments = frame[5], body = length - HeaderBytes;
        Received &packet = received[node - 1];
        if (fragments < 1 || fragments > Plan.fragments || index >= fragments ||
            index != packet.next || (index + 1 < fragments && body != Plan.chunk) ||
            (index > 0 && (fragments != packet.fragments || frame[1] != packet.format))) {
            continue;
        }
        memcpy(payloads[node - 1] + packet.bytes, frame + HeaderBytes, body);
        packet.bytes += body;
        packet.format = frame[1];
        packet.fragments = fragments;
        packet.rssi = radio.lastRssi();
        packet.snr = radio.lastSNR();
        ++packet.next;
    }
}

void processRound() {
#if BENCH_ENABLE_LTE
    bool canPublish = true;
#endif
    for (uint8_t i = 0; i < Plan.nodes; ++i) {
        const Received &packet = received[i];
        NodeStats &total = stats[i];
        if (packet.next == 0 || packet.next != packet.fragments) {
            total.partial += packet.next > 0;
            continue;
        }

        auto &sample = manager.getDocument();
        const uint8_t *encoded = payloads[i];
        const DeserializationError error = packet.format == Json
            ? deserializeJson(sample, encoded, packet.bytes)
            : deserializeMsgPack(sample, encoded, packet.bytes);
        const char *name = sample["id"]["name"].as<const char *>();
        if (error || !manager.isPacketValid() || name == nullptr || name[0] == '\0' ||
            strlen(name) >= Manager::DEVICE_NAME_SIZE ||
            !sample["id"]["instance"].is<int>() || sample["id"]["instance"].as<int>() != i + 1) {
            ++total.invalid;
            continue;
        }

        ++total.good;
        total.bytes += packet.bytes;
        total.rssi = packet.rssi;
        total.snr = packet.snr;
        if (PrintFirstSample && !printed) {
            printed = true;
            Serial.print(F("sample,"));
            serializeJson(sample, Serial);
            Serial.println();
        }
#if BENCH_ENABLE_LTE
        if (canPublish) {
            manager.set_device_name(name);
            manager.set_instance_num(i + 1);
            const uint32_t started = millis();
            canPublish = mqtt.publish();
            lteMs += millis() - started;
            total.acked += canPublish;
        }
#endif
    }
}

void report() {
    const uint32_t now = millis();
    if (now - reportStart - lastReport < ReportMs) {
        return;
    }
    lastReport = now - reportStart;
    const float seconds = lastReport / 1000.0f;
    const float rfSeconds = radioMs / 1000.0f;
    uint32_t good = 0, bytes = 0, acked = 0;

    Serial.println(F("node,expected,good,missed,partial,bad_doc,payload_B,last_RSSI_dBm,last_SNR_dB,mqtt_ack"));
    for (uint8_t i = 0; i < Plan.nodes; ++i) {
        const NodeStats &s = stats[i];
        good += s.good;
        bytes += s.bytes;
        acked += s.acked;
        Serial.print(i + 1); Serial.print(',');
        Serial.print(rounds); Serial.print(',');
        Serial.print(s.good); Serial.print(',');
        Serial.print(rounds - s.good); Serial.print(',');
        Serial.print(s.partial); Serial.print(',');
        Serial.print(s.invalid); Serial.print(',');
        Serial.print(s.bytes); Serial.print(',');
        Serial.print(s.rssi); Serial.print(',');
        Serial.print(s.snr); Serial.print(',');
        Serial.println(s.acked);
    }

    Serial.print(F("wall_s=")); Serial.print(seconds);
    Serial.print(F(" radio_round_s=")); Serial.print(rfSeconds);
    Serial.print(F(" mqtt_call_s=")); Serial.print(lteMs / 1000.0f);
    Serial.print(F(" LoRa_samples/s=")); Serial.print(rfSeconds > 0 ? good / rfSeconds : 0);
    Serial.print(F(" LoRa_payload_B/s=")); Serial.print(rfSeconds > 0 ? bytes / rfSeconds : 0);
    Serial.print(F(" wall_samples/s=")); Serial.print(good / seconds);
    Serial.print(F(" wall_payload_B/s=")); Serial.print(bytes / seconds);
    Serial.print(F(" delivery_pct=")); Serial.print(rounds ? (100.0f * good / rounds) / Plan.nodes : 0);
#if BENCH_ENABLE_LTE
    Serial.print(F(" mqtt_ack/s=")); Serial.print(acked / seconds);
    Serial.print(F(" mqtt_unacked=")); Serial.print(good - acked);
#endif
    Serial.print(F(" beacon_failures=")); Serial.println(beaconFailures);
    (void)acked;
}

void loop() {
    if (!ready) {
        delay(1000);
        return;
    }

    memset(received, 0, sizeof(received));
    uint8_t beacon[BeaconBytes] = {NetworkId, Beacon, 0, 0, Plan.nodes, Plan.chunk, Plan.fragments, 0, 0};
    const uint16_t round = uint16_t(rounds + 1);
    write16(beacon + 2, round);
    write16(beacon + 7, Plan.leadMs);

    const uint32_t started = millis();
    if (sendFrame(radio, beacon, sizeof(beacon))) {
        receiveRound(round, millis());
        radioMs += millis() - started;
        ++rounds;
        processRound();
    } else {
        ++beaconFailures;
        delay(100);
    }
    report();
    delay(IdleAfterRoundMs);
}
