#pragma once

#include <cmath>

namespace bench {
namespace hub {

constexpr Schedule DefaultPlan{2, MaxChunk, 1, 120};
constexpr size_t BufferBytes = 8192;
constexpr uint8_t FitHeadroom = 16, CalibrationAttempts = 3;
constexpr uint32_t ReportMs = 10000;
constexpr uint32_t LiveCheckMs = 15000;
constexpr uint8_t LiveMissLimit = 3;

Manager manager("LoRaHub", 0);

#if BENCH_ENABLE_LTE
Loom_LTE lte(manager, BENCH_APN, BENCH_APN_USER, BENCH_APN_PASS,
             A5, BENCH_LTE_BOARD, -1, BENCH_LTE_MODEM);
Loom_MongoDB mqtt(manager, lte, BENCH_BROKER, BENCH_PORT, BENCH_DATABASE,
                 BENCH_MQTT_USER, BENCH_MQTT_PASS);
#endif

struct Received {
    uint16_t bytes;
    FragmentSet parts;
    int16_t rssi, snr;
    bool valid;
};

struct NodeStats {
    uint32_t good, bytes, acked, partial, invalid, firstGoodRound;
    uint32_t firstMissing, firstPartial, recovered;
    uint32_t expected, firstGoodExpected, skippedDeadlines, empty;
    uint16_t maxBytes;
    int16_t rssi, snr;
};

struct DataVolume {
    uint32_t rxFrameBytes, populatedFields, estimatedBytes;

    void count(bool populated, uint8_t width) {
        if (populated) {
            ++populatedFields;
            estimatedBytes += width;
        }
    }
};

struct Settings {
    Schedule plan;
    uint16_t padBytes, gapMs;
    uint8_t format, source;
    bool upload;
    uint8_t retries;
};

void printSettings(const Settings &settings, Print &output = Serial);
void printStats(Print &output = Serial);
void beginRun(const Settings &settings, uint32_t limit, bool allowPeriods = false);
void serviceEnrollment();

Settings config{DefaultPlan, 0, 0, MsgPack, SyntheticSample, false, MaxRetries};
Settings active = config;
uint8_t payloads[BufferBytes];
Received received[MaxNodes];
NodeStats stats[MaxNodes];
DataVolume volume{};
uint32_t rounds = 0, radioMs = 0, lteMs = 0, beaconFailures = 0;
uint32_t maxRoundMs = 0, maxPollFinishMs = 0;
uint32_t roundRxFrameBytes = 0, retryWindows = 0, retrySlots = 0;
uint32_t retryRadioMs = 0, retryBeaconFailures = 0;
uint32_t runStarted = 0, runElapsed = 0, lastReport = 0, lastRoundEnd = 0, roundLimit = 0;
uint32_t armedRounds = 0;
uint8_t armReadyCount = 0;
uint16_t sequence = 0;
bool running = false, hasRun = false, compareJsonNext = false;
bool live = false, liveCheckDue = false;
uint32_t lastLiveCheck = 0, liveCheckMs = 0;
uint16_t livePeak[MaxNodes];
uint8_t liveMisses[MaxNodes];
uint8_t hubId = 1, runHubId = 1;
bool offering = true;
uint32_t nextOffer = 0;
bool enrollment = false;
EnrollmentSlots<MaxNodes> leases;
uint32_t nextJoinWindow = 0;
uint16_t joinWindow = 0;
uint32_t uploadPeriods[MaxNodes] = {};
PeriodicDeadline uploadDeadlines[MaxNodes];
SlotMask pacedNodes = 0;
uint32_t nextPacedControl = 0;
uint32_t lastQuietGrant = 0;
uint16_t outstandingQuietMs = 0;
#if BENCH_ENABLE_LTE
bool modemStarted = false;
#endif

uint8_t *slotBuffer(uint8_t index) {
    return payloads + size_t(index) * active.plan.chunk * active.plan.fragments;
}

bool setUploadPeriod(uint8_t node, uint32_t periodMs) {
    if (running || compareJsonNext || node > MaxNodes || !validPeriod(periodMs)) {
        return false;
    }
    if (node) {
        uploadPeriods[node - 1] = periodMs;
    } else {
        for (uint32_t &period : uploadPeriods) {
            period = periodMs;
        }
    }
    return true;
}

void printPeriods() {
    Serial.println(F("node,upload_period_ms"));
    for (uint8_t i = 0; i < config.plan.nodes; ++i) {
        Serial.print(i + 1); Serial.print(','); Serial.println(uploadPeriods[i]);
    }
}

void sendPacedControl(SlotMask nodes, uint16_t quietMs) {
    uint8_t frame[PacedControlBytes] = {NetworkId, PacedControl, static_cast<uint8_t>(enrollment),
                                      active.source};
    writeSlotMask(frame + 4, nodes);
    write16(frame + PacedQuietOffset, quietMs);
    sendFrame(radio, RH_BROADCAST_ADDRESS, frame, sizeof(frame));
    if (quietMs) {
        lastQuietGrant = millis();
        outstandingQuietMs = quietMs;
    }
}

SlotMask takeDueNodes(uint32_t now, uint32_t &startDelayMs) {
    SlotMask due = 0;
    for (uint8_t i = 0; i < active.plan.nodes; ++i) {
        PeriodicDeadline &clock = uploadDeadlines[i];
        const uint32_t deadlines = clock.take(now);
        if (deadlines) {
            due |= SlotMask(1) << i;
            stats[i].skippedDeadlines += deadlines - 1;
            // The most recent due deadline gets the slot. Older deadlines were skipped.
            startDelayMs = max(startDelayMs, now - (clock.next - clock.period));
        }
    }
    return due;
}

void servicePacedIdle(uint32_t now) {
    uint32_t wait = UINT32_MAX;
    for (uint8_t i = 0; i < active.plan.nodes; ++i) {
        wait = min(wait, uploadDeadlines[i].waitMs(now));
    }
    const uint32_t joinBudget = airtimeMs(JoinOfferBytes) + FrameMarginMs + JoinSlots * joinSlotMs();
    if (enrollment && wait > joinBudget) {
        serviceEnrollment();
        const uint32_t afterJoin = millis();
        wait -= min(wait, afterJoin - now);
        now = afterJoin;
    }
    const uint32_t overhead = airtimeMs(PacedControlBytes) + 2 * GuardMs;
    if (static_cast<int32_t>(now - nextPacedControl) >= 0 && wait > overhead + LocalReadBudgetMs) {
        // This is a quiet grant, not a data round. Leave time for the next beacon
        // even when the transmit completion and receive clocks differ slightly.
        const uint16_t quiet = static_cast<uint16_t>(min(wait - overhead, uint32_t(MaxQuietMs)));
        sendPacedControl(pacedNodes, quiet);
        nextPacedControl = millis() + 2000;
    }
    if (!roundLimit && millis() - lastReport >= ReportMs) {
        lastReport = millis();
        printStats();
    }
}

void activate() {
#if !BENCH_ENABLE_LTE
    if (!manager.isInitialized()) {
        manager.initialize();
    }
#endif
    nextOffer = millis() + 110UL * hubId;
    nextJoinWindow = millis();
}

void serviceEnrollment() {
    // Open contention slots only between data rounds, never during an uplink slot.
    if (!enrollment || (!running && !offering) ||
        static_cast<int32_t>(millis() - nextJoinWindow) < 0) {
        return;
    }
    uint8_t offer[JoinOfferBytes] = {NetworkId, JoinOffer, 0, 0};
    write16(offer + 2, ++joinWindow);
    nextJoinWindow = millis() + (running ? 10000UL : 2600UL);
    if (!sendFrame(radio, RH_BROADCAST_ADDRESS, offer, sizeof(offer))) {
        return;
    }

    const uint32_t start = millis();
    const uint32_t slot = joinSlotMs();
    while (millis() - start < JoinSlots * slot) {
        uint8_t frame[JoinFrameBytes + 1];
        uint8_t length = sizeof(frame);
        if (!radio.recv(frame, &length)) {
            delay(1);
            continue;
        }
        if (length != JoinFrameBytes || radio.headerFrom() != JoinAddress ||
            radio.headerTo() != hubAddress(hubId) || frame[0] != NetworkId ||
            frame[1] != JoinRequest || read16(frame + 2) != joinWindow) {
            continue;
        }
        const BoardKey key = readBoardKey(frame + JoinOfferBytes + 1);
        const uint32_t bucketStart = joinBucket(key, joinWindow) * slot;
        const uint32_t elapsed = millis() - start;
        if (!key || elapsed < bucketStart ||
            elapsed + PingTurnaroundMs + airtimeMs(JoinFrameBytes) + FrameMarginMs >= bucketStart + slot) {
            continue;
        }
        frame[1] = JoinGrant;
        frame[JoinOfferBytes] = leases.assign(key, frame[JoinOfferBytes], config.plan.nodes);
        delay(PingTurnaroundMs);
        sendFrame(radio, JoinAddress, frame, JoinFrameBytes);
    }
}

void serviceDiscovery() {
    uint8_t frame[PingBytes + 1]; // One extra byte distinguishes an overlength request.
    uint8_t length = sizeof(frame);
    if (radio.recv(frame, &length)) {
        const uint8_t node = radio.headerFrom();
        if (length == PingBytes && node >= 1 && node <= MaxNodes &&
            (!enrollment || leases.keyAt(node)) &&
            radio.headerTo() == hubAddress(hubId) && frame[0] == NetworkId && frame[1] == Ping) {
            frame[1] = Pong;
            delay(PingTurnaroundMs);
            sendFrame(radio, node, frame, PingBytes);
        }
    }
    if (!enrollment && offering && static_cast<int32_t>(millis() - nextOffer) >= 0) {
        const uint8_t offer[OfferBytes] = {NetworkId, Offer};
        sendFrame(radio, RH_BROADCAST_ADDRESS, offer, sizeof(offer));
        // Different periods keep hubs that boot together from repeatedly colliding.
        nextOffer = millis() + 2200UL + 137UL * hubId;
    }
}

void receiveRound(uint16_t round, uint32_t beaconEnd, SlotMask slots, uint16_t leadMs) {
    const Schedule &plan = active.plan;
    const uint32_t slotLength = slotMs(plan);
    const uint32_t roundEnd = leadMs + slotCount(slots) * slotLength;
    while (millis() - beaconEnd < roundEnd) {
        uint8_t frame[RH_RF95_MAX_MESSAGE_LEN];
        uint8_t length = sizeof(frame);
        if (!radio.recv(frame, &length)) {
            continue;
        }
        const uint32_t elapsed = millis() - beaconEnd;
        const uint8_t node = radio.headerFrom();
        if (node < 1 || node > plan.nodes || (enrollment && !leases.keyAt(node)) ||
            radio.headerTo() != hubAddress(runHubId) ||
            length <= HeaderBytes || length > HeaderBytes + plan.chunk ||
            frame[0] != NetworkId || frame[1] != active.format ||
            read16(frame + 2) != round) {
            continue;
        }
        const uint8_t position = retrySlot(slots, node);
        if (!position) {
            continue;
        }
        const uint32_t start = leadMs + (position - 1) * slotLength;
        if (elapsed < start || elapsed >= start + slotLength) {
            continue;
        }
        const uint8_t index = frame[4], fragments = frame[5], body = length - HeaderBytes;
        Received &packet = received[node - 1];
        if (fragments > plan.fragments || !packet.parts.matches(index, fragments) ||
            (index + 1 < fragments && body != plan.chunk)) {
            continue;
        }
        // Count airtime payload from every accepted frame, even a duplicate fragment.
        roundRxFrameBytes += length + RH_RF95_HEADER_LEN;
        if (!packet.parts.add(index, fragments)) {
            continue;
        }
        memcpy(slotBuffer(node - 1) + size_t(index) * plan.chunk, frame + HeaderBytes, body);
        packet.bytes += body;
        packet.rssi = radio.lastRssi();
        packet.snr = radio.lastSNR();
    }
}

SlotMask recordFirstPass(SlotMask due) {
    SlotMask loss = 0;
    for (uint8_t i = 0; i < active.plan.nodes; ++i) {
        const FragmentSet &parts = received[i].parts;
        if (!(due & (SlotMask(1) << i))) {
            continue;
        }
        ++stats[i].expected;
        if (!parts.complete()) {
            loss |= SlotMask(1) << i;
            if (parts.count) {
                ++stats[i].firstPartial;
            } else {
                ++stats[i].firstMissing;
            }
        }
    }
    return loss;
}

void retryMissing(uint16_t round, SlotMask due) {
    for (uint8_t attempt = 1; attempt <= active.retries; ++attempt) {
        uint8_t request[RetryHeaderBytes + MaxNodes] = {NetworkId, RetryRequest};
        request[RetryAttemptOffset] = attempt;
        uint8_t length = RetryHeaderBytes;
        SlotMask slots = 0;
        for (uint8_t i = 0; i < active.plan.nodes; ++i) {
            if ((due & (SlotMask(1) << i)) && !received[i].parts.complete() &&
                (!enrollment || leases.keyAt(i + 1))) {
                slots |= SlotMask(1) << i;
                request[length++] = received[i].parts.missing();
            }
        }
        if (!slots) {
            return;
        }
        write16(request + 2, round);
        writeSlotMask(request + 4, slots);
        const uint32_t started = millis();
        ++retryWindows;
        retrySlots += slotCount(slots);
        if (!sendFrame(radio, RH_BROADCAST_ADDRESS, request, length)) {
            ++retryBeaconFailures;
        }
        // A timed-out send may still have reached a node. Drain the scheduled window
        // before issuing another beacon so a late reply cannot overlap a new round.
        receiveRound(round, millis(), slots, RetryLeadMs);
        retryRadioMs += millis() - started;
    }
}

bool decodeSample(uint8_t index) {
    auto &sample = manager.getDocument();
    const uint8_t *encoded = slotBuffer(index);
    const uint16_t bytes = received[index].bytes;
    const DeserializationError error = active.format == Json
        ? deserializeJson(sample, encoded, bytes)
        : deserializeMsgPack(sample, encoded, bytes);
    const char *name = sample["id"]["name"].as<const char *>();
    return !error && manager.isPacketValid() && name && name[0] &&
           strlen(name) < Manager::DEVICE_NAME_SIZE && sample["id"]["instance"].is<int>() &&
           sample["id"]["instance"].as<int>() == index + 1;
}

void countPopulatedData(JsonArrayConst contents, uint8_t source) {
    const auto finiteNumber = [](JsonVariantConst field) {
        return field.is<float>() && std::isfinite(field.as<float>());
    };
    const auto encoderCode = [](JsonVariantConst field) {
        return field.is<uint16_t>() && field.as<uint16_t>() < 4096;
    };

    JsonObjectConst climate, encoder, displacement;
    for (JsonObjectConst entry : contents) {
        const char *module = entry["module"].as<const char *>();
        if (!module) {
            continue;
        }
        const JsonObjectConst data = entry["data"].as<JsonObjectConst>();
        if (source == SyntheticSample && !strcmp(module, "Bench")) {
            volume.count(finiteNumber(data["Temperature_C"]), 4);
            volume.count(finiteNumber(data["Humidity_pct"]), 4);
            return;
        }
        if (source == SensorSample) {
            if (climate.isNull() && !strcmp(module, "SHT31")) {
                climate = data;
            } else if (encoder.isNull() && !strcmp(module, "AS5311")) {
                encoder = data;
            } else if (displacement.isNull() && !strcmp(module, "displacement")) {
                displacement = data;
            }
        }
    }

    if (source == SensorSample) {
        // Estimate a packed record: float32 climate and displacement, uint16 encoder codes,
        // and one byte for the alignment category. The wire format is unchanged.
        volume.count(finiteNumber(climate["Temperature_C"]), 4);
        volume.count(finiteNumber(climate["Humidity_%RH"]), 4);
        volume.count(encoderCode(encoder["mag"]), 2);
        volume.count(encoderCode(encoder["pos_raw"]), 2);
        volume.count(encoderCode(encoder["pos_avg"]), 2);
        volume.count(finiteNumber(displacement["um"]), 4);
        const char *alignment = encoder["Alignment"].as<const char *>();
        volume.count(alignment && (!strcmp(alignment, "Red") || !strcmp(alignment, "Yellow") ||
                                   !strcmp(alignment, "Green")), 1);
    }
}

void processRound(SlotMask firstLoss, SlotMask due) {
    volume.rxFrameBytes += roundRxFrameBytes;
#if BENCH_ENABLE_LTE
    bool canPublish = active.upload;
#endif
    for (uint8_t i = 0; i < active.plan.nodes; ++i) {
        if (!(due & (SlotMask(1) << i))) {
            continue;
        }
        Received &packet = received[i];
        NodeStats &total = stats[i];
        if (!packet.parts.complete()) {
            total.partial += packet.parts.count > 0;
            continue;
        }

        packet.valid = decodeSample(i);
        if (!packet.valid) {
            ++total.invalid;
            continue;
        }

        if (total.good == 0) {
            total.firstGoodRound = rounds;
            total.firstGoodExpected = total.expected;
        }
        ++total.good;
        total.recovered += (firstLoss & (SlotMask(1) << i)) != 0;
        total.bytes += packet.bytes;
        total.maxBytes = max(total.maxBytes, packet.bytes);
        total.rssi = packet.rssi;
        total.snr = packet.snr;
        const char *type = manager.getDocument()["type"].as<const char *>();
        if (pacedNodes && active.source == SensorSample && type && !strcmp(type, "empty")) {
            ++total.empty;
            continue;
        }
        if (active.source != ProbeSample) {
            countPopulatedData(manager.getDocument()["contents"].as<JsonArrayConst>(), active.source);
        }
#if BENCH_ENABLE_LTE
        if (canPublish) {
            manager.set_device_name(manager.getDocument()["id"]["name"].as<const char *>());
            manager.set_instance_num(i + 1);
            const uint32_t started = millis();
            canPublish = mqtt.publish();
            lteMs += millis() - started;
            total.acked += canPublish;
        }
#endif
    }
}

void printSettings(const Settings &settings, Print &output) {
    const Schedule &plan = settings.plan;
    output.print(F("nodes=")); output.print(plan.nodes);
    output.print(F(" chunk_B=")); output.print(plan.chunk);
    output.print(F(" fragments=")); output.print(plan.fragments);
    output.print(F(" lead_ms=")); output.print(plan.leadMs);
    output.print(F(" slot_ms=")); output.print(slotMs(plan));
    output.print(F(" gap_ms=")); output.print(settings.gapMs);
    output.print(F(" pad_B=")); output.print(settings.padBytes);
    output.print(F(" format=")); output.print(settings.format == Json ? F("json") : F("msgpack"));
    output.print(F(" source="));
    output.print(settings.source == ProbeSample ? F("probe") :
                 settings.source == SyntheticSample ? F("synthetic") : F("sensors"));
    output.print(F(" lte=")); output.print(settings.upload ? F("on") : F("off"));
    output.print(F(" retries=")); output.print(settings.retries);
    output.print(F(" bw_kHz=")); output.println(WideBandwidth ? 500 : 125);
}

void printStats(Print &output) {
    if (!hasRun) {
        output.println(F("ERR no run yet"));
        return;
    }
    output.print(F("hub=")); output.print(runHubId); output.print(' ');
    printSettings(active, output);
    const float seconds = (running ? millis() - runStarted : runElapsed) / 1000.0f;
    const float rfSeconds = radioMs / 1000.0f;
    uint32_t good = 0, bytes = 0, acked = 0, expectedAfterJoin = 0;
    uint32_t expected = 0, empty = 0, skippedDeadlines = 0;
    uint32_t firstMissing = 0, firstPartial = 0, recovered = 0;
    uint32_t finalPartial = 0, finalInvalid = 0;

    output.println(F("node,expected,good,missed,partial,bad_doc,payload_B,max_sample_B,last_RSSI_dBm,last_SNR_dB,mqtt_ack,first_good_round,expected_since_join,first_missing,first_partial,recovered,final_missing,empty,skipped_deadlines"));
    for (uint8_t i = 0; i < active.plan.nodes; ++i) {
        const NodeStats &s = stats[i];
        good += s.good;
        bytes += s.bytes;
        acked += s.acked;
        expected += s.expected;
        empty += s.empty;
        skippedDeadlines += s.skippedDeadlines;
        firstMissing += s.firstMissing;
        firstPartial += s.firstPartial;
        recovered += s.recovered;
        finalPartial += s.partial;
        finalInvalid += s.invalid;
        const uint32_t sinceJoin = s.firstGoodExpected ? s.expected - s.firstGoodExpected + 1 : 0;
        expectedAfterJoin += sinceJoin;
        output.print(i + 1); output.print(',');
        output.print(s.expected); output.print(',');
        output.print(s.good); output.print(',');
        output.print(s.expected - s.good); output.print(',');
        output.print(s.partial); output.print(',');
        output.print(s.invalid); output.print(',');
        output.print(s.bytes); output.print(',');
        output.print(s.maxBytes); output.print(',');
        if (s.good) { output.print(s.rssi); }
        output.print(',');
        if (s.good) { output.print(s.snr); }
        output.print(',');
        output.print(s.acked); output.print(',');
        if (s.firstGoodRound) { output.print(s.firstGoodRound); }
        output.print(',');
        output.print(sinceJoin); output.print(',');
        output.print(s.firstMissing); output.print(',');
        output.print(s.firstPartial); output.print(',');
        output.print(s.recovered); output.print(',');
        output.print(s.expected - s.good - s.partial - s.invalid); output.print(',');
        output.print(s.empty); output.print(',');
        output.println(s.skippedDeadlines);
    }

    const uint32_t samples = good - empty;
    output.print(F("wall_s=")); output.print(seconds);
    output.print(F(" radio_round_s=")); output.print(rfSeconds);
    output.print(F(" mqtt_call_s=")); output.print(lteMs / 1000.0f);
    if (liveCheckMs) {
        output.print(F(" live_check_s=")); output.print(liveCheckMs / 1000.0f);
    }
    output.print(F(" LoRa_samples/s=")); output.print(rfSeconds > 0 ? samples / rfSeconds : 0);
    output.print(F(" LoRa_payload_B/s=")); output.print(rfSeconds > 0 ? bytes / rfSeconds : 0);
    output.print(F(" wall_samples/s=")); output.print(seconds > 0 ? samples / seconds : 0);
    output.print(F(" wall_payload_B/s=")); output.print(seconds > 0 ? bytes / seconds : 0);
    output.print(F(" delivery_pct=")); output.print(expected ? 100.0f * good / expected : 0);
    if (pacedNodes) {
        output.print(F(" empty_replies=")); output.print(empty);
        output.print(F(" skipped_deadlines=")); output.print(skippedDeadlines);
    }
    if (expectedAfterJoin) {
        output.print(F(" delivery_since_join_pct=")); output.print(100.0f * good / expectedAfterJoin);
    }
    if (active.upload) {
        output.print(F(" mqtt_ack/s=")); output.print(seconds > 0 ? acked / seconds : 0);
        output.print(F(" mqtt_unacked=")); output.print(samples - acked);
    }
    output.print(F(" polls=")); output.print(rounds + beaconFailures);
    output.print(F(" round_max_ms=")); output.print(maxRoundMs);
    output.print(F(" poll_finish_max_ms=")); output.print(maxPollFinishMs);
    output.print(F(" beacon_failures=")); output.println(beaconFailures);
    output.print(F("first_missing=")); output.print(firstMissing);
    output.print(F(" first_partial=")); output.print(firstPartial);
    output.print(F(" recovered=")); output.print(recovered);
    output.print(F(" final_missing="));
    output.print(expected - good - finalPartial - finalInvalid);
    output.print(F(" final_partial=")); output.print(finalPartial);
    output.print(F(" final_invalid=")); output.print(finalInvalid);
    output.print(F(" retry_windows=")); output.print(retryWindows);
    output.print(F(" retry_slots=")); output.print(retrySlots);
    output.print(F(" retry_beacon_failures=")); output.print(retryBeaconFailures);
    output.print(F(" retry_radio_s=")); output.println(retryRadioMs / 1000.0f);
    if (active.source != ProbeSample) {
        const uint8_t fieldsPerSample = active.source == SensorSample ? 7 : 2;
        output.print(F("populated_fields=")); output.print(volume.populatedFields);
        output.print(F(" expected_fields_in_good_samples=")); output.print(samples * fieldsPerSample);
        output.print(F(" LoRa_rx_frame_B/s="));
        output.print(rfSeconds > 0 ? volume.rxFrameBytes / rfSeconds : 0);
        output.print(F(" LoRa_populated_fields/s="));
        output.print(rfSeconds > 0 ? volume.populatedFields / rfSeconds : 0);
        output.print(F(" LoRa_est_data_B/s="));
        output.print(rfSeconds > 0 ? volume.estimatedBytes / rfSeconds : 0);
        output.print(F(" wall_rx_frame_B/s="));
        output.print(seconds > 0 ? volume.rxFrameBytes / seconds : 0);
        output.print(F(" wall_populated_fields/s="));
        output.print(seconds > 0 ? volume.populatedFields / seconds : 0);
        output.print(F(" wall_est_data_B/s="));
        output.println(seconds > 0 ? volume.estimatedBytes / seconds : 0);
    }
}

void printSample(uint8_t node) {
    for (uint8_t i = 0; i < active.plan.nodes; ++i) {
        if ((node == 0 || node == i + 1) && received[i].valid && decodeSample(i)) {
            serializeJson(manager.getDocument(), Serial);
            Serial.println();
            return;
        }
    }
    Serial.println(F("ERR no matching valid sample in the last received round"));
}

void beginRun(const Settings &settings, uint32_t limit, bool allowPeriods) {
    active = settings;
    runHubId = hubId;
    roundLimit = limit;
    memset(stats, 0, sizeof(stats));
    memset(received, 0, sizeof(received));
    volume = {};
    rounds = radioMs = lteMs = beaconFailures = 0;
    maxRoundMs = maxPollFinishMs = 0;
    roundRxFrameBytes = retryWindows = retrySlots = retryRadioMs = retryBeaconFailures = 0;
    liveCheckMs = 0;
    Serial.print(armedRounds ? F("ARM hub=") : F("BEGIN hub=")); Serial.println(runHubId);
    printSettings(active);
    runStarted = lastReport = millis();
    pacedNodes = 0;
    nextPacedControl = runStarted;
    for (uint8_t i = 0; i < active.plan.nodes; ++i) {
        const uint32_t interval = allowPeriods ? uploadPeriods[i] : 0;
        uploadDeadlines[i].reset(runStarted, interval);
        if (interval) {
            pacedNodes |= SlotMask(1) << i;
        }
    }
    if (pacedNodes) {
        Serial.println(F("PACED fixed deadlines; gap setting ignored"));
        printPeriods();
    }
    lastRoundEnd = runStarted - active.gapMs;
    runElapsed = 0;
    hasRun = running = true;
}

void finishRun() {
    runElapsed = millis() - runStarted;
    running = false;
    live = false;
    if (pacedNodes && ready) {
        sendPacedControl(0, 0);
    }
    Serial.println(armedRounds ? F("ARM stopped") : F("END"));
    armedRounds = 0;
    printStats();
}

// A programmatic equivalent of selecting the source and using start/run on USB.
bool startCollection(uint8_t source, uint32_t limit = 0) {
    if (role != Role::Hub || !ready || running || compareJsonNext ||
        source > SyntheticSample || limit > 100000) {
        return false;
    }
    Settings test = config;
    test.source = source;
    live = limit == 0;
    if (live) {
        memset(livePeak, 0, sizeof(livePeak));
        memset(liveMisses, 0, sizeof(liveMisses));
        liveCheckDue = true;
    }
    beginRun(test, limit, true);
    return true;
}

uint16_t calibrationPeak() {
    uint16_t peak = 0;
    for (uint8_t i = 0; i < active.plan.nodes; ++i) {
        if (!stats[i].good) {
            return 0;
        }
        peak = max(peak, stats[i].maxBytes);
    }
    return peak;
}

bool findShortestSlot(Schedule &plan, uint32_t target) {
    Schedule best = plan;
    uint32_t bestMs = UINT32_MAX;
    for (uint16_t chunk = 1; chunk <= MaxChunk; ++chunk) {
        const uint32_t fragments = (target + chunk - 1) / chunk;
        const uint32_t capacity = chunk * fragments;
        if (fragments > MaxFragments || capacity * best.nodes > BufferBytes) {
            continue;
        }
        const Schedule candidate{best.nodes, static_cast<uint8_t>(chunk),
                                 static_cast<uint8_t>(fragments), best.leadMs};
        const uint32_t ms = slotMs(candidate);
        // Prefer fewer frames, then the larger chunk when both have the same slot time.
        if (ms < bestMs || (ms == bestMs && candidate.fragments <= best.fragments)) {
            best = candidate;
            bestMs = ms;
        }
    }
    if (bestMs == UINT32_MAX) {
        return false;
    }
    plan = best;
    return true;
}

bool fitSlots(uint32_t headroom) {
    if (running || compareJsonNext) {
        Serial.println(F("ERR stop the run before fitting slots"));
        return false;
    }
    if (!hasRun || active.source == ProbeSample || active.plan.nodes != config.plan.nodes ||
        active.source != config.source || active.format != config.format ||
        active.padBytes != config.padBytes) {
        Serial.println(F("ERR first run a benchmark with the current nodes, source, format and pad"));
        return false;
    }
    const uint16_t peak = calibrationPeak();
    if (!peak) {
        Serial.println(F("ERR need a sample from every configured node; see stats"));
        return false;
    }
    const uint32_t oldSlotMs = slotMs(config.plan);
    if (!findShortestSlot(config.plan, peak + headroom)) {
        Serial.println(F("ERR sample plus headroom exceeds the fragment or hub memory limit"));
        return false;
    }

    Serial.print(F("FIT peak_B=")); Serial.print(peak);
    Serial.print(F(" headroom_B=")); Serial.print(headroom);
    Serial.print(F(" old_slot_ms=")); Serial.println(oldSlotMs);
    printSettings(config);
    return true;
}

Schedule calibrationPlan(Schedule plan, uint8_t fragments) {
    plan.fragments = min(MaxFragments, fragments);
    plan.chunk = static_cast<uint8_t>(min(size_t(MaxChunk), BufferBytes / (plan.nodes * plan.fragments)));
    return plan;
}

Settings calibrationSettings() {
    Settings calibration = config;
    const uint8_t padFragments = (config.padBytes + MaxChunk - 1) / MaxChunk;
    const uint8_t minFragments = (config.source == SensorSample ? 2 : 1) + padFragments;
    calibration.plan = calibrationPlan(config.plan, max(config.plan.fragments, minFragments));
    calibration.gapMs = 0;
    calibration.upload = false;
    return calibration;
}

bool advanceArmedRun() {
    if (active.source != ProbeSample) {
        const uint16_t peak = calibrationPeak();
        if (!peak) {
            if (rounds + beaconFailures < CalibrationAttempts) {
                if (rounds == 1) {
                    for (uint8_t fragments = 1; fragments <= MaxFragments; ++fragments) {
                        const Schedule candidate = calibrationPlan(active.plan, fragments);
                        if (candidate.chunk * candidate.fragments > active.plan.chunk * active.plan.fragments) {
                            active.plan = candidate;
                        }
                    }
                    memset(received, 0, sizeof(received));
                }
                return false;
            }
            Serial.println(F("ERR calibration did not receive every node. See stats and slot capacity."));
            finishRun();
            return true;
        }

        Settings measured = config;
        if (!findShortestSlot(measured.plan, peak + FitHeadroom)) {
            Serial.println(F("ERR calibration peak plus headroom exceeds available capacity"));
            finishRun();
            return true;
        }
        const uint32_t limit = armedRounds;
        armedRounds = 0;
        Serial.print(F("ARM fitted peak_B=")); Serial.print(peak);
        Serial.print(F(" slot_ms=")); Serial.println(slotMs(measured.plan));
        beginRun(measured, limit);
        return true;
    }

    uint8_t available = 0;
    for (uint8_t i = 0; i < active.plan.nodes; ++i) {
        available += received[i].valid;
    }
    if (available != armReadyCount) {
        armReadyCount = available;
        Serial.print(F("ARM ready=")); Serial.print(available);
        Serial.print('/'); Serial.println(active.plan.nodes);
    }
    if (available != active.plan.nodes) {
        return false;
    }

    Serial.println(F("ARM calibrating packet sizes"));
    beginRun(calibrationSettings(), 0);
    return true;
}

void setOption(const char *key, const char *arg) {
    Settings next = config;
    uint32_t value;
    if (!strcmp(key, "hub") && parseNumber(arg, 1, MaxHubs, value)) {
        hubId = static_cast<uint8_t>(value);
        setRadioAddress(radio, hubAddress(hubId));
        nextOffer = millis() + 110UL * hubId;
        nextJoinWindow = millis();
        Serial.print(F("OK hub=")); Serial.println(hubId);
        return;
    } else if (!strcmp(key, "offer") && (!strcmp(arg, "on") || !strcmp(arg, "off"))) {
        offering = !strcmp(arg, "on");
        nextOffer = millis() + 110UL * hubId;
        Serial.println(offering ? F("OK offer=on") : F("OK offer=off"));
        return;
    } else if (!strcmp(key, "enrollment") && (!strcmp(arg, "on") || !strcmp(arg, "off"))) {
        enrollment = !strcmp(arg, "on");
        nextJoinWindow = millis();
        Serial.println(enrollment ? F("OK enrollment=on") : F("OK enrollment=off"));
        return;
    } else if (!strcmp(key, "forget") && parseNumber(arg, 1, MaxNodes, value)) {
        Serial.println(leases.forget(static_cast<uint8_t>(value)) ?
                           F("OK slot reservation removed") : F("ERR no reservation for that slot"));
        return;
    } else if (!strcmp(key, "nodes") && parseNumber(arg, 1, MaxNodes, value)) {
        if (value < leases.highestReserved()) {
            Serial.println(F("ERR forget slots above the new limit before reducing nodes"));
            return;
        }
        next.plan.nodes = static_cast<uint8_t>(value);
    } else if (!strcmp(key, "chunk") && parseNumber(arg, 1, MaxChunk, value)) {
        next.plan.chunk = static_cast<uint8_t>(value);
    } else if (!strcmp(key, "fragments") && parseNumber(arg, 1, MaxFragments, value)) {
        next.plan.fragments = static_cast<uint8_t>(value);
    } else if (!strcmp(key, "lead") && parseNumber(arg, GuardMs, 10000, value)) {
        next.plan.leadMs = static_cast<uint16_t>(value);
    } else if (!strcmp(key, "gap") && parseNumber(arg, 0, 60000, value)) {
        next.gapMs = static_cast<uint16_t>(value);
    } else if (!strcmp(key, "retries") && parseNumber(arg, 0, MaxRetries, value)) {
        next.retries = static_cast<uint8_t>(value);
    } else if (!strcmp(key, "pad") && parseNumber(arg, 0, MaxPadding, value)) {
        next.padBytes = static_cast<uint16_t>(value);
    } else if (!strcmp(key, "format") && (!strcmp(arg, "msgpack") || !strcmp(arg, "json"))) {
        next.format = !strcmp(arg, "json") ? Json : MsgPack;
    } else if (!strcmp(key, "source") && (!strcmp(arg, "synthetic") || !strcmp(arg, "sensors"))) {
        next.source = !strcmp(arg, "synthetic") ? SyntheticSample : SensorSample;
    } else if (!strcmp(key, "lte") && (!strcmp(arg, "on") || !strcmp(arg, "off"))) {
        next.upload = !strcmp(arg, "on");
#if BENCH_ENABLE_LTE
        if (next.upload && !modemStarted) {
            Serial.println(F("Starting LTE modem..."));
            if (BENCH_LTE_SUPPLY_PIN >= 0) {
                digitalWrite(BENCH_LTE_SUPPLY_PIN, HIGH);
                pinMode(BENCH_LTE_SUPPLY_PIN, OUTPUT);
                delay(1000);
            }
            manager.initialize();
            modemStarted = true;
        }
#else
        if (next.upload) {
            Serial.println(F("ERR LTE support is disabled in this build"));
            return;
        }
#endif
    } else {
        Serial.println(F("ERR unknown setting or out-of-range value. Type help."));
        return;
    }
    if (size_t(next.plan.nodes) * next.plan.chunk * next.plan.fragments > BufferBytes) {
        Serial.println(F("ERR nodes * chunk * fragments exceeds 8192 bytes"));
        return;
    }
    config = next;
    printSettings(config);
}

void armRun(uint32_t count) {
    const Settings probe{{config.plan.nodes, 96, 1, 30}, 0, 700, MsgPack, ProbeSample, false, 0};
    armedRounds = count;
    armReadyCount = 0;
    Serial.print(F("ARM waiting for IDs 1-")); Serial.println(config.plan.nodes);
    beginRun(probe, 0);
}

void handleCommand(char *line) {
    const char *verb = strtok(line, " \t");
    const char *arg = strtok(nullptr, " \t");
    const char *value = strtok(nullptr, " \t");
    if (!verb) {
        return;
    }
    if (!strcmp(verb, "set") && arg && !strcmp(arg, "period")) {
        const char *duration = strtok(nullptr, " \t");
        uint32_t node = 0, period = 0;
        if (!value || (strcmp(value, "all") && !parseNumber(value, 1, MaxNodes, node)) ||
            !parseNumber(duration, 0, MaxPeriodMs, period) || strtok(nullptr, " \t") ||
            !setUploadPeriod(static_cast<uint8_t>(node), period)) {
            Serial.println(F("ERR use set period <node|all> <0 or 100-86400000 ms> while idle"));
        } else {
            printPeriods();
        }
        return;
    }
    if (!strcmp(verb, "set") && arg && value && !strtok(nullptr, " \t")) {
        if (running || compareJsonNext) {
            Serial.println(F("ERR stop the run before changing settings"));
        } else {
            setOption(arg, value);
        }
        return;
    }
    if (value) {
        Serial.println(F("ERR too many arguments. Type help."));
        return;
    }

    uint32_t count;
    if (!arg && !strcmp(verb, "help")) {
        Serial.println(F("role | role node | role hub"));
        Serial.println(F("arm [rounds] (wait, fit slots automatically, then benchmark)"));
        Serial.println(F("fit [headroom_B] (shortest slot from last run, default 16 B)"));
        Serial.println(F("scan [rounds] | run [rounds] | compare [rounds per format] | start | stop"));
        Serial.println(F("start fits slots automatically in unpaced mode"));
        Serial.println(F("status | stats | sample [node ID] | help"));
        Serial.println(F("set hub 1-16 | set offer on|off (idle advertisements)"));
        Serial.println(F("set enrollment on|off | leases | set forget <slot>"));
        Serial.println(F("set nodes 1-20 | set chunk 1-245 | set fragments 1-8 | set lead 12-10000 (ms)"));
        Serial.println(F("set gap 0-60000 (ms) | set pad 0-1800 | set format msgpack|json"));
        Serial.println(F("set retries 0-3 (missing fragments only; default 3)"));
        Serial.println(F("set period <node|all> <ms> | periods (0 clears a period)"));
        Serial.println(F("set source synthetic|sensors | set lte on|off"));
#if BENCH_ENABLE_SWEEP
        Serial.println(F("sweep usb|start [hours] | sweep status | sweep stop (1000 polls per test)"));
#endif
        Serial.println(F("Defaults: scan 2 rounds, arm/run/compare 10 rounds; settings last until reset."));
    } else if (!arg && !strcmp(verb, "status")) {
        Serial.print(F("hub=")); Serial.print(hubId);
        Serial.print(offering ? F(" offer=on ") : F(" offer=off "));
        Serial.print(ready ? F("radio=ready") : F("radio=failed"));
        Serial.print(armedRounds ? F(" armed ") : live ? F(" live ") :
                     running ? F(" running ") : F(" idle "));
        Serial.print(BENCH_ENABLE_LTE ? F("lte_build=yes ") : F("lte_build=no "));
        Serial.print(enrollment ? F("enrollment=on ") : F("enrollment=off "));
        printSettings(running ? active : config);
    } else if (!arg && !strcmp(verb, "leases")) {
        Serial.print(F("reserved_slots="));
        for (uint8_t slot = 1; slot <= config.plan.nodes; ++slot) {
            if (leases.keyAt(slot)) {
                Serial.print(' '); Serial.print(slot);
            }
        }
        Serial.println();
    } else if (!arg && !strcmp(verb, "stats")) {
        printStats();
    } else if (!arg && !strcmp(verb, "periods")) {
        printPeriods();
    } else if (!strcmp(verb, "fit")) {
        count = FitHeadroom;
        if (!arg || parseNumber(arg, 0, MaxChunk, count)) {
            fitSlots(count);
        } else {
            Serial.println(F("ERR fit headroom must be 0-245 bytes"));
        }
    } else if (!strcmp(verb, "sample") && (!arg || parseNumber(arg, 1, MaxNodes, count))) {
        printSample(arg ? static_cast<uint8_t>(count) : 0);
    } else if (!arg && !strcmp(verb, "stop")) {
        compareJsonNext = false;
        if (running) {
            finishRun();
        } else {
            Serial.println(F("OK idle"));
        }
    } else if ((!arg && !strcmp(verb, "start")) || !strcmp(verb, "run") || !strcmp(verb, "arm") ||
               !strcmp(verb, "scan") || !strcmp(verb, "compare")) {
        const bool scanning = !strcmp(verb, "scan");
        const bool arming = !strcmp(verb, "arm");
        count = scanning ? 2 : 10;
        if (arg && !parseNumber(arg, 1, 100000, count)) {
            Serial.println(F("ERR round count must be 1-100000"));
        } else if (running || compareJsonNext || !ready) {
            Serial.println(ready ? F("ERR run already active") : F("ERR radio unavailable"));
        } else if (arming) {
            armRun(count);
        } else if (scanning) {
            const Settings scan{{MaxNodes, 96, 1, 30}, 0, 0, MsgPack, ProbeSample, false, 0};
            beginRun(scan, count);
        } else if (!strcmp(verb, "compare")) {
            Settings test = config;
            live = false;
            compareJsonNext = true;
            test.format = MsgPack;
            beginRun(test, count, true);
        } else {
            startCollection(config.source, !strcmp(verb, "start") ? 0 : count);
        }
    } else {
        Serial.println(F("ERR invalid command. Type help."));
    }
}

void deactivate() {
    compareJsonNext = false;
    if (running) {
        finishRun();
    }
}

bool exchangeRound(SlotMask due, bool paced = false) {
    const Schedule &plan = active.plan;
    const uint8_t kind = paced ? (enrollment ? PacedEnrolledBeacon : PacedBeacon) :
                                (enrollment ? EnrolledBeacon : Beacon);
    uint8_t beacon[MaxBeaconBytes] = {NetworkId, kind,
                                    0, 0, plan.nodes, plan.chunk, plan.fragments,
                                    0, 0, active.format, active.source, 0, 0};
    const uint16_t round = ++sequence;
    write16(beacon + 2, round);
    write16(beacon + 7, plan.leadMs);
    write16(beacon + 11, active.padBytes);

    uint8_t beaconLength = BeaconBytes;
    if (paced) {
        writeSlotMask(beacon + beaconLength, due);
        beaconLength += SlotMaskBytes;
    }
    if (enrollment) {
        for (uint8_t slot = 1; slot <= plan.nodes; ++slot) {
            writeBoardKey(beacon + beaconLength, leases.keyAt(slot));
            beaconLength += BoardKeyBytes;
        }
    }
    if (!sendFrame(radio, RH_BROADCAST_ADDRESS, beacon, beaconLength)) {
        // Some nodes may have heard a beacon even when our TX completion timed out.
        delay(plan.leadMs + slotCount(due) * slotMs(plan));
        return false;
    }
    const uint32_t beaconEnd = millis();
    memset(received, 0, sizeof(received));
    roundRxFrameBytes = 0;
    receiveRound(round, beaconEnd, due, plan.leadMs);
    return true;
}

void noteLiveSample(uint8_t index) {
    if (!livePeak[index]) {
        Serial.print(F("LIVE joined node=")); Serial.println(index + 1);
    }
    livePeak[index] = max(livePeak[index], received[index].bytes);
    liveMisses[index] = 0;
}

void fitLiveSlots() {
    uint16_t peak = 0;
    for (uint8_t i = 0; i < active.plan.nodes; ++i) {
        peak = max(peak, livePeak[i]);
    }
    Schedule plan = active.plan;
    if (!peak || (!findShortestSlot(plan, peak + FitHeadroom) && !findShortestSlot(plan, peak)) ||
        (plan.chunk == active.plan.chunk && plan.fragments == active.plan.fragments)) {
        return;
    }
    active.plan = plan;
    memset(received, 0, sizeof(received)); // Layout depends on the slot capacity.
    Serial.print(F("LIVE fitted peak_B=")); Serial.print(peak);
    Serial.print(F(" slot_ms=")); Serial.println(slotMs(plan));
}

void trackLiveRound() {
    bool refit = false;
    for (uint8_t i = 0; i < active.plan.nodes; ++i) {
        if (received[i].valid) {
            refit |= received[i].bytes > livePeak[i];
            noteLiveSample(i);
        } else if (livePeak[i] && ++liveMisses[i] >= LiveMissLimit) {
            livePeak[i] = 0;
            refit = liveCheckDue = true;
            Serial.print(F("LIVE missing node=")); Serial.println(i + 1);
        }
    }
    if (refit) {
        fitLiveSlots();
    }
}

void checkLiveNodes() {
    const uint32_t started = millis();
    const Settings previous = active;
    active = calibrationSettings();
    active.plan = calibrationPlan(active.plan, max(active.plan.fragments, previous.plan.fragments));
    if (size_t(previous.plan.chunk) * previous.plan.fragments >
        size_t(active.plan.chunk) * active.plan.fragments) {
        active.plan = previous.plan;
    }
    if (exchangeRound(slotMask(active.plan.nodes))) {
        for (uint8_t i = 0; i < active.plan.nodes; ++i) {
            const Received &packet = received[i];
            if (packet.parts.complete() && decodeSample(i)) {
                noteLiveSample(i);
            }
        }
    }
    active = previous;
    memset(received, 0, sizeof(received));
    fitLiveSlots();
    lastRoundEnd = lastLiveCheck = millis();
    liveCheckMs += lastLiveCheck - started;
    liveCheckDue = false;
}

void loop() {
    if (compareJsonNext && !running) {
        compareJsonNext = false;
        Settings second = active;
        second.format = Json;
        beginRun(second, roundLimit, true);
    }
    if (millis() - lastQuietGrant < outstandingQuietMs) {
        // Honor an advertised quiet window even across stop/start, in case a
        // node missed the stop message while it was reading its sensors.
        delay(1);
        return;
    }
    outstandingQuietMs = 0;
    if (ready && (!running || !pacedNodes)) {
        serviceEnrollment();
    }
    if (!running) {
        if (ready) {
            serviceDiscovery();
        }
        delay(1);
        return;
    }
    if (!pacedNodes && millis() - lastRoundEnd < active.gapMs) {
        delay(1);
        return;
    }
    if (live && !pacedNodes) {
        uint8_t present = 0;
        for (uint8_t i = 0; i < active.plan.nodes; ++i) {
            present += livePeak[i] != 0;
        }
        if (liveCheckDue ||
            (present < active.plan.nodes && millis() - lastLiveCheck >= LiveCheckMs + active.gapMs)) {
            checkLiveNodes();
            return;
        }
        if (!present) {
            delay(1);
            return;
        }
    }

    const uint32_t started = millis();
    uint32_t startDelayMs = 0;
    const SlotMask due = pacedNodes ? takeDueNodes(started, startDelayMs) : slotMask(active.plan.nodes);
    if (!due) {
        servicePacedIdle(started);
        delay(1);
        return;
    }
    if (exchangeRound(due, pacedNodes != 0)) {
        const SlotMask firstLoss = recordFirstPass(due);
        if (firstLoss && active.retries && active.source != ProbeSample) {
            retryMissing(sequence, due);
        }
        radioMs += millis() - started;
        ++rounds;
        processRound(firstLoss, due);
        if (live && !pacedNodes) {
            trackLiveRound();
        }
    } else {
        ++beaconFailures;
        delay(100);
    }
    const uint32_t elapsed = millis() - started;
    maxRoundMs = max(maxRoundMs, elapsed);
    maxPollFinishMs = max(maxPollFinishMs, startDelayMs + elapsed);
    if (armedRounds && advanceArmedRun()) {
        return;
    }
    lastRoundEnd = millis();
    if (roundLimit && rounds + beaconFailures >= roundLimit) {
        finishRun();
    } else if (!roundLimit && millis() - lastReport >= ReportMs) {
        lastReport = millis();
        printStats();
    }
}

} // namespace hub

} // namespace bench
