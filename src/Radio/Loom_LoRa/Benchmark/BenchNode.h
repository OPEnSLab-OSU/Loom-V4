#pragma once

namespace bench {
namespace node {

constexpr uint32_t ScanMs = 8000, OfferAgeMs = 15000, PingTimeoutMs = 1000;
constexpr uint32_t PreferHubMs = 5000, HubSilenceMs = 120000;
constexpr uint16_t LocalSampleBytes = 384;
constexpr uint8_t LocalQueueSize = 4;

uint8_t nodeId = 1, preferredId = 1, preferredHub = 1, selectedHub = 0;
bool autoHub = true;
bool enrollment = false;
BoardKey boardKey = 0;
uint32_t searchStarted = 0, lastHubContact = 0;
Manager manager("LoRaNode", 1);
#if BENCH_ENABLE_DENDROMETER
DendrometerSensors dendrometer(manager);
#endif
uint8_t payload[MaxChunk * MaxFragments + 1]; // Allow a JSON terminator at full capacity.
char padding[MaxPadding + 1] = {};
uint32_t beacons = 0, sent = 0, skipped = 0;
uint32_t retriesSent = 0, retriesFailed = 0;
uint32_t slotBudgetMisses = 0, radioTxFailures = 0, oversizeSamples = 0;
uint32_t linkRetries = 0;
uint32_t linkRecovered = 0, linkFailed = 0, linkFailedAttempts = 0;
uint16_t lastRound = 0;
size_t jsonBytes = 0, packedBytes = 0;
const __FlashStringHelper *lastResult = F("waiting for beacon");

struct CachedSample {
    Schedule plan;
    uint32_t beaconEnd;
    size_t bytes;
    uint16_t round;
    uint8_t format, hub, slot, lastRetry, firstSlots, source;
    bool individual;
};

CachedSample cached{};

struct LocalSample {
    uint16_t size;
    uint8_t data[LocalSampleBytes];
};

LocalSample localQueue[LocalQueueSize];
uint8_t localHead = 0, localCount = 0;
uint32_t localPeriod = 0, localCaptured = 0, localDropped = 0, localMissed = 0;
PeriodicDeadline localClock;
uint32_t quietStart = 0;
uint16_t quietLength = 0;
uint16_t individualReadBudgetMs = LocalReadBudgetMs;
uint32_t localOverruns = 0;
bool localActive = false, localIndividual = false;

struct HeardHub {
    uint32_t lastSeen;
    int16_t rssi, snr;
    bool seen;
};

HeardHub heard[MaxHubs];
uint32_t scanStarted = 0, pingStarted = 0;
uint16_t pingToken = 0;
bool scanning = false, pingPending = false;

void printStatus() {
    Serial.print(F("node=")); Serial.print(nodeId);
    Serial.print(F(" hub="));
    if (selectedHub) {
        Serial.print(selectedHub);
    } else {
        Serial.print(F("searching"));
    }
    Serial.print(autoHub ? F(" hub_mode=auto") : F(" hub_mode=manual"));
    Serial.print(F(" preferred_hub=")); Serial.print(preferredHub);
    Serial.print(F(" radio=")); Serial.print(ready ? F("ready") : F("failed"));
    Serial.print(F(" bw_kHz=")); Serial.print(WideBandwidth ? 500 : 125);
    Serial.print(F(" beacons=")); Serial.print(beacons);
    Serial.print(F(" round=")); Serial.print(lastRound);
    Serial.print(F(" json_B=")); Serial.print(jsonBytes);
    Serial.print(F(" msgpack_B=")); Serial.print(packedBytes);
    Serial.print(F(" sent=")); Serial.print(sent);
    Serial.print(F(" skipped=")); Serial.print(skipped);
    Serial.print(F(" retries_sent=")); Serial.print(retriesSent);
    Serial.print(F(" retries_failed=")); Serial.print(retriesFailed);
    Serial.print(F(" slot_budget_misses=")); Serial.print(slotBudgetMisses);
    Serial.print(F(" radio_tx_failures=")); Serial.print(radioTxFailures);
    Serial.print(F(" link_retries=")); Serial.print(linkRetries);
    Serial.print(F(" link_recovered_fragments=")); Serial.print(linkRecovered);
    Serial.print(F(" link_failed_fragments=")); Serial.print(linkFailed);
    Serial.print(F(" link_failed_attempts=")); Serial.print(linkFailedAttempts);
    Serial.print(F(" oversize_samples=")); Serial.print(oversizeSamples);
    Serial.print(enrollment ? F(" id_mode=auto") : F(" id_mode=fixed"));
    Serial.print(F(" preferred_id=")); Serial.print(preferredId);
    Serial.print(F(" name=")); Serial.print(manager.get_device_name());
    Serial.print(F(" local_ms=")); Serial.print(localPeriod);
    Serial.print(F(" queued=")); Serial.print(localCount);
    Serial.print(F(" local_captured=")); Serial.print(localCaptured);
    Serial.print(F(" local_dropped=")); Serial.print(localDropped);
    Serial.print(F(" local_skipped_deadlines=")); Serial.print(localMissed);
    Serial.print(F(" sample_budget_ms=")); Serial.print(individualReadBudgetMs);
    Serial.print(F(" sample_overruns=")); Serial.print(localOverruns);
    Serial.print(F(" result=")); Serial.println(lastResult);
}

void printHubs() {
    const uint32_t now = millis();
    Serial.print(F("selected_hub=")); Serial.println(selectedHub);
    Serial.println(F("hub,last_seen_ms,RSSI_dBm,SNR_dB"));
    bool found = false;
    for (uint8_t i = 0; i < MaxHubs; ++i) {
        HeardHub &hub = heard[i];
        const uint32_t age = now - hub.lastSeen;
        hub.seen = hub.seen && age <= OfferAgeMs;
        if (!hub.seen) {
            continue;
        }
        found = true;
        Serial.print(i + 1); Serial.print(',');
        Serial.print(age); Serial.print(',');
        Serial.print(hub.rssi); Serial.print(',');
        Serial.println(hub.snr);
    }
    if (!found) {
        Serial.println(F("No recent hub offers or test beacons."));
    }
}

void pingHub() {
    if (!ready || scanning || pingPending) {
        Serial.println(F("ERR ping needs a ready radio and no pending scan or ping"));
        return;
    }
    if (!selectedHub) {
        Serial.println(F("ERR still looking for a hub. Use hubs or hub <id>."));
        return;
    }
    if (!nodeId) {
        Serial.println(F("ERR waiting for a slot from the hub"));
        return;
    }
    uint8_t request[PingBytes] = {NetworkId, Ping, 0, 0};
    write16(request + 2, ++pingToken);
    pingStarted = millis();
    pingPending = sendFrame(radio, hubAddress(selectedHub), request, sizeof(request));
    if (!pingPending) {
        Serial.println(F("ERR ping TX failed"));
    }
}

bool hearHub(uint8_t sender, uint32_t receivedAt) {
    heard[sender - 1] = {receivedAt, radio.lastRssi(), static_cast<int16_t>(radio.lastSNR()), true};
    if (autoHub && !selectedHub && !scanning &&
        (sender == preferredHub || receivedAt - searchStarted >= PreferHubMs)) {
        selectedHub = sender;
    }
    if (sender == selectedHub) {
        lastHubContact = receivedAt;
        return true;
    }
    return false;
}

void resetStats() {
    beacons = sent = skipped = 0;
    retriesSent = retriesFailed = 0;
    slotBudgetMisses = radioTxFailures = oversizeSamples = linkRetries = 0;
    linkRecovered = linkFailed = linkFailedAttempts = 0;
    cached.bytes = 0;
    localActive = localIndividual = false;
    quietLength = 0;
    localHead = localCount = 0;
    localCaptured = localDropped = localMissed = localOverruns = 0;
    lastRound = 0;
    jsonBytes = packedBytes = 0;
    lastResult = F("waiting for beacon");
    pingPending = false;
}

bool setLocalSamplePeriod(uint32_t periodMs) {
    if (!validPeriod(periodMs)) {
        return false;
    }
    localPeriod = periodMs;
    localClock.reset(millis(), periodMs);
    localHead = localCount = 0;
    if (!periodMs) {
        localActive = localIndividual = false;
        quietLength = 0;
    }
    return true;
}

void setLocalActive(bool enabled, uint32_t now, bool individual = false) {
    enabled = enabled && localPeriod;
    individual = enabled && individual;
    if (localActive != enabled || localIndividual != individual) {
        localActive = enabled;
        localIndividual = individual;
        localClock.reset(now, enabled ? localPeriod : 0);
        quietLength = 0;
        localHead = localCount = 0;
    }
}

void applySlot(uint8_t slot) {
    if (nodeId != slot) {
        cached.bytes = 0;
    }
    nodeId = slot;
    manager.set_instance_num(slot ? slot : preferredId);
    if (ready) {
        setRadioAddress(radio, slot ? slot : JoinAddress);
    }
}

bool setEnrollment(bool enabled) {
    if (enabled && !boardKey) {
        boardKey = boardFingerprint(manager.get_serial_num());
        if (!boardKey) {
            Serial.println(F("ERR no hardware identity for automatic enrollment"));
            return false;
        }
    }
    if ((enrollment || enabled) && autoHub) {
        selectedHub = 0;
    }
    enrollment = enabled;
    applySlot(enabled ? JoinAddress : preferredId);
    manager.getDocument().clear();
    resetStats();
    searchStarted = lastHubContact = millis();
    return true;
}

void selectHub(uint8_t destination) {
    autoHub = destination == 0;
    selectedHub = destination;
    searchStarted = millis();
    if (enrollment) {
        applySlot(JoinAddress);
    }
    manager.getDocument().clear();
    resetStats();
    printStatus();
}

#if BENCH_ENABLE_DENDROMETER
void printEncoderWord(const __FlashStringHelper *label, uint32_t word) {
    Serial.print(label); Serial.print(F(" word=0x")); Serial.print(word, HEX);
    if (word == 0 || word == 0x3FFFFUL) {
        Serial.println(F(" constant data"));
        return;
    }
    const bool valid = AS5311::isValidReading(word);
    Serial.print(F(" value=")); Serial.print(word >> DATAOFFSET);
    Serial.print(F(" OCF=")); Serial.print((word >> OCF) & 1U);
    Serial.print(F(" COF=")); Serial.print((word >> COF) & 1U);
    Serial.print(F(" LIN=")); Serial.print((word >> LIN) & 1U);
    if (!valid && (word & (1UL << OCF)) && !(word & (1UL << COF))) {
        Serial.print(F(" parity_error"));
    }
    Serial.println();
}
#endif

void handleCommand(char *line) {
    const char *verb = strtok(line, " \t");
    const char *arg = strtok(nullptr, " \t");
    if (!verb) {
        return;
    }
    uint32_t value;
    if (strtok(nullptr, " \t")) {
        Serial.println(F("ERR too many arguments"));
    } else if (!strcmp(verb, "id") && arg && !strcmp(arg, "auto")) {
        if (setEnrollment(true)) {
            printStatus();
        }
    } else if (!strcmp(verb, "id") && parseNumber(arg, 1, MaxNodes, value)) {
        preferredId = static_cast<uint8_t>(value);
        if (setEnrollment(false)) {
            printStatus();
        }
    } else if (!strcmp(verb, "name") && arg && strlen(arg) < Manager::DEVICE_NAME_SIZE) {
        manager.set_device_name(arg);
        manager.getDocument().clear();
        cached.bytes = 0;
        printStatus();
    } else if (!strcmp(verb, "interval")) {
        if (!parseNumber(arg, 0, MaxPeriodMs, value) || !setLocalSamplePeriod(value)) {
            Serial.println(F("ERR use interval <0 or 100-86400000 ms>"));
        } else {
            printStatus();
        }
    } else if (!strcmp(verb, "samplebudget") && parseNumber(arg, 1, MaxIndividualQuietMs - GuardMs, value)) {
        individualReadBudgetMs = static_cast<uint16_t>(value);
        printStatus();
    } else if (!strcmp(verb, "hub") && arg && !strcmp(arg, "auto")) {
        selectHub(0);
    } else if (!strcmp(verb, "hub") && parseNumber(arg, 1, MaxHubs, value)) {
        selectHub(static_cast<uint8_t>(value));
    } else if (!arg && !strcmp(verb, "scan")) {
        if (!ready || pingPending) {
            Serial.println(F("ERR scan needs a ready radio and no pending ping"));
            return;
        }
        memset(heard, 0, sizeof(heard));
        scanning = true;
        scanStarted = millis();
        Serial.println(F("Listening for hub offers and beacons for 8 seconds..."));
    } else if (!arg && !strcmp(verb, "hubs")) {
        printHubs();
    } else if (!arg && !strcmp(verb, "ping")) {
        pingHub();
    } else if (!arg && !strcmp(verb, "status")) {
        printStatus();
    } else if (!arg && !strcmp(verb, "reset")) {
        resetStats();
        manager.getDocument().clear();
        printStatus();
    } else if (!arg && !strcmp(verb, "sample")) {
        if (manager.isPacketValid()) {
            serializeJson(manager.getDocument(), Serial);
            Serial.println();
        } else {
            Serial.println(F("ERR no valid sample yet"));
        }
#if BENCH_ENABLE_DENDROMETER
    } else if (!arg && !strcmp(verb, "encoder")) {
        printEncoderWord(F("position"), dendrometer.encoder.getRawData());
        printEncoderWord(F("magnitude"), dendrometer.encoder.getRawMagnitudeData());
#endif
    } else if (!arg && !strcmp(verb, "help")) {
        Serial.println(F("role | role node | role hub"));
        Serial.println(F("scan | hubs | hub auto | hub <1-16> | ping (selected idle hub)"));
        Serial.println(F("id <1-20> | id auto | name <label> | status | sample | reset | help"));
        Serial.println(F("interval <ms> (local sampling in paced sensor runs; 0 samples on request)"));
        Serial.println(F("samplebudget <ms> (worst-case sensor read time for individual polling)"));
#if BENCH_ENABLE_DENDROMETER
        Serial.println(F("encoder (AS5311 diagnostics on an idle node)"));
#endif
        Serial.println(F("Serial settings last until reset. Firmware Boot sets power-on defaults."));
    } else {
        Serial.println(F("ERR invalid command. Type help."));
    }
}

void activate(void (*prepareSensors)()) {
    if (!manager.isInitialized()) {
#if BENCH_ENABLE_DENDROMETER
        dendrometer.setPower(true);
#endif
        if (prepareSensors) {
            prepareSensors();
        }
        manager.initialize();
    }
    if (autoHub && !selectedHub) {
        searchStarted = millis();
    }
    printStatus();
}

void deactivate() {
    scanning = false;
    pingPending = false;
    cached.bytes = 0;
    setLocalActive(false, millis());
}

void requestSlot(uint8_t senderHub, uint16_t window, uint32_t offerEnd) {
    const uint32_t slot = joinSlotMs();
    const uint32_t bucketStart = joinBucket(boardKey, window) * slot;
    while (millis() - offerEnd < bucketStart + GuardMs) {
        delay(1);
    }
    if (millis() - offerEnd + airtimeMs(JoinFrameBytes) + FrameMarginMs >= bucketStart + slot) {
        return;
    }
    uint8_t request[JoinFrameBytes] = {NetworkId, JoinRequest, 0, 0, preferredId};
    write16(request + 2, window);
    writeBoardKey(request + JoinOfferBytes + 1, boardKey);
    if (!sendFrame(radio, hubAddress(senderHub), request, sizeof(request))) {
        return;
    }
    while (millis() - offerEnd < bucketStart + slot) {
        uint8_t reply[JoinFrameBytes + 1];
        uint8_t length = sizeof(reply);
        if (!radio.recv(reply, &length)) {
            delay(1);
            continue;
        }
        if (length != JoinFrameBytes || radio.headerFrom() != hubAddress(senderHub) ||
            radio.headerTo() != JoinAddress || reply[0] != NetworkId || reply[1] != JoinGrant ||
            read16(reply + 2) != window || readBoardKey(reply + JoinOfferBytes + 1) != boardKey) {
            continue;
        }
        const uint8_t granted = reply[JoinOfferBytes];
        if (granted && granted <= MaxNodes) {
            applySlot(granted);
            lastHubContact = millis();
            Serial.print(F("JOIN hub=")); Serial.print(senderHub);
            Serial.print(F(" preferred=")); Serial.print(preferredId);
            Serial.print(F(" slot=")); Serial.println(nodeId);
        } else {
            lastResult = F("hub has no free slot");
        }
        return;
    }
}

uint8_t cachedFragmentCount() {
    return static_cast<uint8_t>((cached.bytes + cached.plan.chunk - 1) / cached.plan.chunk);
}

void readSensors() {
    manager.measure();
    manager.package();
#if BENCH_ENABLE_DENDROMETER
    dendrometer.encoder.measure(manager);
#endif
}

void popLocalSample() {
    if (localCount) {
        localHead = (localHead + 1) % LocalQueueSize;
        --localCount;
    }
}

bool captureLocalSample(uint32_t now) {
    const uint32_t deadlines = localActive ? localClock.take(now) : 0;
    if (!deadlines) {
        return false;
    }
    localMissed += deadlines - 1;
    readSensors();
    auto &sample = manager.getDocument();
    sample["sample_ms"] = now;
    const size_t bytes = measureMsgPack(sample);
    if (!manager.isPacketValid() || !bytes || bytes > LocalSampleBytes) {
        ++localDropped;
        return true;
    }
    if (localCount == LocalQueueSize) {
        popLocalSample();
        ++localDropped;
    }
    const uint8_t index = (localHead + localCount) % LocalQueueSize;
    LocalSample &slot = localQueue[index];
    if (serializeMsgPack(sample, slot.data, bytes) != bytes) {
        ++localDropped;
        return true;
    }
    slot.size = static_cast<uint16_t>(bytes);
    if (localIndividual) {
        localHead = index;
        localCount = 1;
    } else {
        ++localCount;
    }
    ++localCaptured;
    return true;
}

void serviceLocalSampling(uint32_t now) {
    const uint32_t elapsed = now - quietStart;
    const uint32_t budget = localIndividual ? individualReadBudgetMs : LocalReadBudgetMs;
    const uint32_t guard = localIndividual ? GuardMs : 0;
    if (elapsed < quietLength && quietLength - elapsed >= budget + guard &&
        captureLocalSample(now)) {
        if (localIndividual && millis() - now > budget) ++localOverruns;
        // A slow sensor may overrun its quiet grant. Do not treat a beacon
        // received during that read as if it had just arrived.
        radio.recv(nullptr, nullptr);
    }
}

bool sendWithFragmentAck(uint8_t destination, uint8_t *frame, uint8_t length, const LinkSettings &link) {
    for (uint8_t attempt = 0; attempt <= link.retries; ++attempt) {
        if (attempt) ++linkRetries;
        if (!sendFrame(radio, destination, frame, length)) continue;
        const uint32_t sentAt = millis();
        while (millis() - sentAt < link.ackMs) {
            uint8_t ack[HeaderBytes + 1];
            uint8_t size = sizeof(ack);
            if (radio.recv(ack, &size) && size == HeaderBytes && radio.headerFrom() == destination &&
                radio.headerTo() == nodeId && ack[0] == NetworkId && ack[1] == FragmentAck &&
                memcmp(ack + 2, frame + 2, HeaderBytes - 2) == 0) return true;
            delay(1);
        }
    }
    return false;
}

bool sendUplink(uint8_t *frame, uint8_t length, const LinkSettings &link) {
    const uint8_t destination = hubAddress(cached.hub);
    const uint32_t before = linkRetries;
    radio.cadMs = link.cadMs;
    bool sentFrame;
    if (link.transport == Transport::RadioHead) {
        reliableRadio.setThisAddress(nodeId);
        reliableRadio.setRetries(link.retries);
        reliableRadio.setTimeout(link.ackMs);
        const uint32_t priorRadioHeadRetries = reliableRadio.retransmissions();
        sentFrame = reliableRadio.sendtoWait(frame, length, destination);
        linkRetries += reliableRadio.retransmissions() - priorRadioHeadRetries;
    } else if (link.transport == Transport::Ack) {
        sentFrame = sendWithFragmentAck(destination, frame, length, link);
    } else {
        sentFrame = sendFrame(radio, destination, frame, length);
    }
    radio.cadMs = 0;
    if (link.transport != Transport::Basic) {
        if (sentFrame) {
            linkRecovered += linkRetries != before;
        } else {
            ++linkFailed;
            linkFailedAttempts += 1 + linkRetries - before;
        }
    }
    return sentFrame;
}

const __FlashStringHelper *sendCached(uint8_t *frame, uint8_t position, uint8_t fragmentsWanted,
                                     uint32_t windowStart, uint16_t leadMs) {
    const Schedule &plan = cached.plan;
    const uint32_t slot = leadMs + (position - 1) * slotMs(plan);
    const uint32_t start = slot + GuardMs;
    const uint32_t end = slot + slotMs(plan) - GuardMs / 2;
    while (millis() - windowStart < start) {
        delay(1);
    }
    const uint8_t fragments = cachedFragmentCount();
    for (uint8_t i = 0; i < fragments; ++i) {
        if (!(fragmentsWanted & (1u << i))) {
            continue;
        }
        const size_t offset = size_t(i) * plan.chunk;
        const uint8_t count = static_cast<uint8_t>(min(size_t(plan.chunk), cached.bytes - offset));
        const uint8_t frameBytes = HeaderBytes + count;
        if (millis() - windowStart + linkFrameMs(frameBytes, plan.link) > end) {
            ++slotBudgetMisses;
            return F("slot budget");
        }
        frame[0] = NetworkId;
        frame[1] = cached.format;
        write16(frame + 2, cached.round);
        frame[4] = i;
        frame[5] = fragments;
        memcpy(frame + HeaderBytes, payload + offset, count);
        if (!sendUplink(frame, frameBytes, plan.link)) {
            ++radioTxFailures;
            return F("radio TX failed");
        }
    }
    return nullptr;
}

void handleRetry(uint8_t *frame, uint8_t length, uint8_t senderHub, uint32_t requestEnd) {
    if (length < RetryHeaderBytes || scanning || pingPending || !cached.bytes || cached.individual ||
        senderHub != selectedHub || senderHub != cached.hub || nodeId != cached.slot ||
        read16(frame + 2) != cached.round || frame[RetryAttemptOffset] <= cached.lastRetry ||
        frame[RetryAttemptOffset] > MaxRetries) {
        return;
    }
    const SlotMask slots = readSlotMask(frame + 4);
    const uint8_t position = retrySlot(slots, nodeId);
    if (!position || (slots & ~slotMask(cached.plan.nodes)) ||
        length != RetryHeaderBytes + slotCount(slots)) {
        return;
    }
    const uint32_t allSlotsMs = cached.plan.nodes * slotMs(cached.plan);
    const uint32_t age = requestEnd - cached.beaconEnd;
    const uint32_t firstPassMs = cached.plan.leadMs + cached.firstSlots * slotMs(cached.plan);
    const uint32_t retryMaxMs = RetryLeadMs + allSlotsMs + airtimeMs(RetryHeaderBytes + MaxNodes) +
                                FrameMarginMs;
    if (age < firstPassMs - GuardMs || age > firstPassMs + MaxRetries * retryMaxMs + GuardMs) {
        return;
    }
    const uint8_t wanted = frame[RetryHeaderBytes + position - 1] &
                           fragmentMask(cachedFragmentCount());
    if (!wanted) {
        return;
    }
    cached.lastRetry = frame[RetryAttemptOffset];
    lastHubContact = requestEnd;
    const __FlashStringHelper *error = sendCached(frame, position, wanted, requestEnd, RetryLeadMs);
    if (error) {
        ++retriesFailed;
        lastResult = error;
    } else {
        ++retriesSent;
        lastResult = F("retry sent");
    }
}

enum class ResponseMode : uint8_t { Slots, Paced, Individual };

void respondToPoll(uint8_t *frame, const SampleRequest &request, uint32_t beaconEnd,
                   uint8_t position, uint8_t firstSlots, ResponseMode mode, uint8_t attempt = 0) {
    const Schedule &schedule = request.plan;
    const uint16_t round = request.round, padBytes = request.padBytes;
    const uint8_t format = request.format, source = request.source;
    const bool individual = mode == ResponseMode::Individual;
    const bool paced = mode == ResponseMode::Paced;
    auto &sample = manager.getDocument();
    bool fromQueue = false;
    if (source == SensorSample && (individual || (paced && localPeriod))) {
        if (!individual) {
            setLocalActive(true, beaconEnd);
            if (!localCount) captureLocalSample(beaconEnd);
        }
        if (localCount && (!individual || localIndividual)) {
            const LocalSample &oldest = localQueue[localHead];
            fromQueue = !deserializeMsgPack(sample, oldest.data, oldest.size) && manager.isPacketValid();
            if (!fromQueue) {
                popLocalSample();
                ++localDropped;
            }
        }
        if (!fromQueue) {
            sample.clear();
            sample["type"] = "empty";
        }
        sample["id"]["name"] = manager.get_device_name();
        sample["id"]["instance"] = manager.get_instance_num();
    } else if (source == SensorSample) {
        readSensors();
    } else {
        sample.clear();
        sample["type"] = source == ProbeSample ? "probe" : "data";
        sample["id"]["name"] = manager.get_device_name();
        sample["id"]["instance"] = manager.get_instance_num();
        if (source == SyntheticSample) {
            manager.addData("Packet", "Number", round);
        }
    }
    if (source != ProbeSample && (source == SyntheticSample || padBytes)) {
        JsonObject data = manager.get_data_object("Bench");
        if (source == SyntheticSample) {
            data["Temperature_C"] = 19.0f + nodeId * 0.25f;
            data["Humidity_pct"] = 50 + nodeId;
        }
        if (padBytes) {
            memset(padding, 'x', padBytes);
            padding[padBytes] = '\0';
            data["Padding"] = static_cast<const char *>(padding);
        }
    }

    jsonBytes = measureJson(sample);
    packedBytes = measureMsgPack(sample);
    const size_t bytes = format == Compact ? CompactBytes : format == Json ? jsonBytes : packedBytes;
    const size_t capacity = size_t(schedule.chunk) * schedule.fragments;
    const __FlashStringHelper *error = nullptr;

    if (!manager.isPacketValid()) {
        error = F("invalid packet");
    } else if (!bytes || bytes > capacity || jsonBytes >= MAX_JSON_SIZE) {
        ++oversizeSamples;
        error = F("oversize");
    } else {
        const size_t written = format == Compact ? packCompact(sample.as<JsonVariantConst>(), nodeId, source, payload)
            : format == Json ? serializeJson(sample, payload, bytes + 1) : serializeMsgPack(sample, payload, bytes);
        if (written != bytes) {
            error = F("encode failed");
        } else {
            cached = {schedule, beaconEnd, bytes, round, format, selectedHub, nodeId,
                      attempt, firstSlots, source, individual};
            if (fromQueue && !individual) {
                popLocalSample(); // The cached bytes remain available for this round's retries.
            }
            error = sendCached(frame, position, fragmentMask(cachedFragmentCount()),
                               beaconEnd, schedule.leadMs);
            if (source == ProbeSample) {
                cached.bytes = 0;
            }
        }
    }

    if (!error) {
        ++sent;
    } else {
        if (fromQueue && !individual && !cached.bytes) {
            popLocalSample(); // An unencodable entry must not block later readings.
            ++localDropped;
        }
        ++skipped;
    }
    lastResult = error ? error : F("sent");
    if (sent + skipped == 1) {
        printStatus();
    }
}

void handleDirectedPoll(uint8_t *frame, uint8_t length, uint8_t senderHub, uint32_t receivedAt) {
    const bool recovery = frame[1] != IndividualPoll;
    const uint8_t linkOffset = enrollment ? MaxIndividualPollBytes : IndividualPollBytes;
    const uint8_t expectedLength = linkOffset + (recovery ? LinkBytes : 0);
    if (length != expectedLength || !nodeId || scanning || pingPending) return;
    SampleRequest request = readSampleRequest(frame);
    if (recovery) request.plan.link = readLinkSettings(frame + linkOffset);
    const uint8_t attempt = frame[PollAttemptOffset], wanted = frame[PollMaskOffset];
    const uint8_t maxAttempt = recovery ? RecordRecoveryPasses : MaxRetries;
    if (!request.valid() || request.source == ProbeSample || nodeId > request.plan.nodes ||
        !wanted || attempt > maxAttempt || (!attempt && (recovery || wanted != UINT8_MAX)) ||
        (recovery && request.plan.link.transport == Transport::Basic) ||
        !hearHub(senderHub, receivedAt)) return;
    if (enrollment && readBoardKey(frame + IndividualPollBytes) != boardKey) {
        applySlot(JoinAddress);
        setLocalActive(false, receivedAt);
        return;
    }

    const uint32_t attemptBudget = request.plan.leadMs + slotMs(request.plan) +
                                  airtimeMs(expectedLength) + FrameMarginMs + 2 * GuardMs;
    const uint32_t cacheLifetime = recovery ? (1UL + RecordRecoveryPasses) * request.plan.nodes * attemptBudget
                                           : (MaxRetries + 1UL) * attemptBudget;
    const bool matchesCache = cached.bytes && cached.individual == !recovery && cached.round == request.round &&
        cached.hub == senderHub && cached.slot == nodeId && cached.format == request.format &&
        cached.source == request.source && cached.plan.chunk == request.plan.chunk &&
        cached.plan.fragments == request.plan.fragments && cached.plan.leadMs == request.plan.leadMs &&
        cached.plan.link.transport == request.plan.link.transport &&
        cached.plan.link.retries == request.plan.link.retries &&
        cached.plan.link.cadMs == request.plan.link.cadMs &&
        cached.plan.link.ackMs == request.plan.link.ackMs &&
        receivedAt - cached.beaconEnd <= cacheLifetime;
    const IndividualReply reply = chooseIndividualReply(matchesCache, attempt, cached.lastRetry, wanted);
    if (reply == IndividualReply::Ignore) return;
    if (reply == IndividualReply::Resend) {
        const uint8_t fragments = wanted & fragmentMask(cachedFragmentCount());
        if (!fragments) return;
        cached.lastRetry = attempt;
        const __FlashStringHelper *error = sendCached(frame, 1, fragments, receivedAt, cached.plan.leadMs);
        if (error) ++retriesFailed;
        else ++retriesSent;
        lastResult = error ? error : F("retry sent");
        return;
    }

    // A repeated full request can recover a missed request. A fragment-only request needs
    // the original encoded bytes so fragments from two readings can never be combined.
    ++beacons;
    lastRound = request.round;
    cached.bytes = 0;
    quietLength = 0;
    const ResponseMode mode = !recovery ? ResponseMode::Individual
                            : frame[1] == PacedRecoveryPoll ? ResponseMode::Paced : ResponseMode::Slots;
    if (request.source != SensorSample || mode == ResponseMode::Slots) setLocalActive(false, receivedAt);
    respondToPoll(frame, request, receivedAt, 1, 1, mode, attempt);
}

void loop() {
    if (!ready) {
        delay(10);
        return;
    }
    const uint32_t now = millis();
    if (localActive && now - lastHubContact >= HubSilenceMs) {
        setLocalActive(false, now);
    }
    if (scanning && now - scanStarted >= ScanMs) {
        scanning = false;
        printHubs();
    }
    if (pingPending && now - pingStarted >= PingTimeoutMs) {
        pingPending = false;
        Serial.print(F("ERR ping timeout hub=")); Serial.println(selectedHub);
    }
    if (autoHub && selectedHub && now - lastHubContact >= HubSilenceMs) {
        selectedHub = 0;
        if (enrollment) {
            applySlot(JoinAddress);
        }
        searchStarted = now;
        manager.getDocument().clear();
        resetStats();
    }

    uint8_t frame[RH_RF95_MAX_MESSAGE_LEN];
    uint8_t length = sizeof(frame);
    if (!radio.recv(frame, &length)) {
        if (!scanning && !pingPending) {
            serviceLocalSampling(now);
        }
        return;
    }
    const uint32_t beaconEnd = millis();
    const uint8_t senderHub = hubIdFromAddress(radio.headerFrom());
    const uint8_t destination = radio.headerTo();
    if (!senderHub || length < OfferBytes || frame[0] != NetworkId) {
        return;
    }
#if BENCH_HAS_RTC_BUTTON
    if (utcSlots::handleRadio(frame, length, senderHub, destination)) return;
#endif
    if (frame[1] == RetryRequest && destination == RH_BROADCAST_ADDRESS) {
        handleRetry(frame, length, senderHub, beaconEnd);
        return;
    }
    if ((frame[1] == IndividualPoll || frame[1] == RecoveryPoll || frame[1] == PacedRecoveryPoll) &&
        destination == nodeId) {
        handleDirectedPoll(frame, length, senderHub, beaconEnd);
        return;
    }
    if ((frame[1] == PacedControl || frame[1] == IndividualControl) &&
        destination == RH_BROADCAST_ADDRESS) {
        const bool individual = frame[1] == IndividualControl;
        const uint16_t maxQuiet = individual ? MaxIndividualQuietMs : MaxQuietMs;
        if (length == PacedControlBytes && frame[2] == static_cast<uint8_t>(enrollment) &&
            frame[3] <= SyntheticSample && read16(frame + PacedQuietOffset) <= maxQuiet &&
            hearHub(senderHub, beaconEnd)) {
            setLocalActive(frame[3] == SensorSample && retrySlot(readSlotMask(frame + 4), nodeId),
                           beaconEnd, individual);
            if (localActive) {
                quietStart = beaconEnd;
                quietLength = read16(frame + PacedQuietOffset);
            }
        }
        return;
    }
    if (enrollment && length == JoinOfferBytes && frame[1] == JoinOffer &&
        destination == RH_BROADCAST_ADDRESS) {
        if (hearHub(senderHub, beaconEnd) && !nodeId && !scanning && !pingPending) {
            requestSlot(senderHub, read16(frame + 2), beaconEnd);
        }
        return;
    }
    if (!enrollment && length == OfferBytes && frame[1] == Offer && destination == RH_BROADCAST_ADDRESS) {
        hearHub(senderHub, beaconEnd);
        return;
    }
    if (pingPending && senderHub == selectedHub && length == PingBytes && frame[1] == Pong && destination == nodeId &&
        read16(frame + 2) == pingToken) {
        pingPending = false;
        lastHubContact = beaconEnd;
        Serial.print(F("OK pong hub=")); Serial.print(senderHub);
        Serial.print(F(" rtt_ms=")); Serial.print(beaconEnd - pingStarted);
        Serial.print(F(" RSSI_dBm=")); Serial.print(radio.lastRssi());
        Serial.print(F(" SNR_dB=")); Serial.println(radio.lastSNR());
        return;
    }
    const bool paced = frame[1] == (enrollment ? PacedEnrolledBeacon : PacedBeacon);
    if (length < BeaconBytes || destination != RH_BROADCAST_ADDRESS ||
        (!paced && frame[1] != (enrollment ? EnrolledBeacon : Beacon))) {
        return;
    }
    SampleRequest request = readSampleRequest(frame);
    const Schedule &schedule = request.plan;
    const uint8_t rosterOffset = BeaconBytes + (paced ? SlotMaskBytes : 0);
    const uint16_t expectedLength = rosterOffset + (enrollment ? schedule.nodes * BoardKeyBytes : 0);
    if (length == expectedLength + LinkBytes) request.plan.link = readLinkSettings(frame + expectedLength);
    else if (length != expectedLength) return;
    if (!request.valid()) {
        return;
    }
    const SlotMask due = paced ? readSlotMask(frame + BeaconBytes) : slotMask(schedule.nodes);
    if (!due || (due & ~slotMask(schedule.nodes)) || (paced && request.source == ProbeSample)) {
        return;
    }
    if (!hearHub(senderHub, beaconEnd) || scanning || pingPending) {
        return;
    }
    ++beacons;
    lastRound = request.round;
    cached.bytes = 0;
    quietLength = 0;
    if (!paced || request.source != SensorSample) {
        setLocalActive(false, beaconEnd);
    }
    if (enrollment && nodeId && nodeId <= schedule.nodes &&
        readBoardKey(frame + rosterOffset + (nodeId - 1) * BoardKeyBytes) != boardKey) {
        // The hub lost or reassigned the reservation; wait for a fresh grant.
        applySlot(JoinAddress);
        setLocalActive(false, beaconEnd);
    }
    if (!nodeId) {
        lastResult = F("waiting for a hub slot");
        return;
    }
    if (nodeId > schedule.nodes) {
        lastResult = F("no slot for this ID");
        return;
    }
    const uint8_t position = retrySlot(due, nodeId);
    if (!position) {
        return; // Not scheduled in this round, so no transmission is expected.
    }

    respondToPoll(frame, request, beaconEnd, position, slotCount(due),
                  paced ? ResponseMode::Paced : ResponseMode::Slots);
}

} // namespace node

} // namespace bench
