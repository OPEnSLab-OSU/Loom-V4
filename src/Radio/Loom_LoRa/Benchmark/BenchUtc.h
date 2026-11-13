#pragma once

#include <OPEnS_RTC.h>
#include <Utilities/Loom_ClockRead.h>
#include "BenchBacklog.h"

namespace bench {
namespace utcSlots {

// Set the same values on the hub and all stations before starting a UTC trial.
#ifndef BENCH_UTC_NODES
#define BENCH_UTC_NODES 2
#endif
#ifndef BENCH_UTC_CYCLE_S
#define BENCH_UTC_CYCLE_S 300
#endif
#ifndef BENCH_UTC_SLOT_S
#define BENCH_UTC_SLOT_S 16
#endif
#ifndef BENCH_UTC_PREP_S
#define BENCH_UTC_PREP_S 15
#endif
#ifndef BENCH_UTC_QUEUE_SLOT_S
#define BENCH_UTC_QUEUE_SLOT_S 24
#endif
#ifndef BENCH_UTC_FORMAT
#define BENCH_UTC_FORMAT bench::Compact
#endif
#ifndef BENCH_UTC_SOURCE
#define BENCH_UTC_SOURCE bench::SensorSample
#endif
#ifndef BENCH_UTC_TRANSPORT
#define BENCH_UTC_TRANSPORT bench::Transport::Ack
#endif

constexpr uint16_t EdgeSeconds = 2;
constexpr uint8_t DrainCycles = 2;
constexpr uint8_t Format = BENCH_UTC_FORMAT, Source = BENCH_UTC_SOURCE;
constexpr uint32_t CycleSeconds = BENCH_UTC_CYCLE_S, SlotSeconds = BENCH_UTC_SLOT_S;
constexpr uint32_t QueueSlotSeconds = BENCH_UTC_QUEUE_SLOT_S;
constexpr Schedule Plan{BENCH_UTC_NODES, Format == Compact ? CompactBytes : MaxChunk,
                        Format == Compact ? 1 : 2, GuardMs,
                        {BENCH_UTC_TRANSPORT, MaxRetries, 100, DefaultAckTimeoutMs}};
static_assert(Plan.valid() && Plan.link.transport != Transport::Basic, "UTC slots need per-frame ACKs");
static_assert(Format == Compact || Format == MsgPack, "Choose raw or MessagePack");
static_assert(Source == SensorSample || Source == SyntheticSample, "Choose live or synthetic samples");
static_assert(BENCH_UTC_PREP_S >= 1 && BENCH_UTC_PREP_S <= 60, "UTC preparation must be 1-60 seconds");
static_assert(SlotSeconds > 2 * EdgeSeconds && Plan.nodes * SlotSeconds <= CycleSeconds &&
              CycleSeconds <= 86400UL, "UTC slots must fit inside one cycle");
static_assert(size_t(Plan.nodes) * Plan.chunk * Plan.fragments <= hub::BufferBytes,
              "UTC receive plan exceeds hub buffer");
static_assert(QueueSlotSeconds > 2 * EdgeSeconds && QueueSlotSeconds <= 86400UL,
              "Queue slots must leave time for both guard edges and fit within a day");

RTC_DS3231 clock;
uint32_t start = 0, count = 0, cycle = 0;
uint32_t cycleSeconds = CycleSeconds, slotSeconds = SlotSeconds;
uint16_t ackTimeoutMs = DefaultAckTimeoutMs;
uint8_t powerDbm = TxPowerDbm, trialId = 0;
bool enabled = false, queueMode = false;

constexpr uint8_t RemoteArm = 0x47, RemoteReady = 0x48, RemoteStop = 0x49;
constexpr uint8_t RemoteStats = 0x4a, RemoteResult = 0x4b;
constexpr uint8_t TokenBytes = 7, ArmBytes = 21, ReadyBytes = 12, ResultBytes = 33;
constexpr uint16_t ControlWaitMs = 550;
constexpr uint8_t ControlAttempts = 4;
bool remoteTrial = false, finished = false;
uint8_t remoteHub = 0;

struct Trial {
    uint32_t epoch, rounds, period;
    uint16_t ack;
    uint8_t power, id;
    bool queue, sd;
};

constexpr uint32_t LatestUtc = 4102444799UL;

bool beginClock() {
#if BENCH_ENABLE_DENDROMETER
    // The hub normally leaves the Hypnos rails off. Enable them before the first I2C access.
    if (role == Role::Hub) node::dendrometer.setPower(true);
#endif
    return clock.begin();
}

bool readUtc(uint32_t &seconds) {
    DateTime stamp;
    if (!loomTime::readEstablishedUtc(clock, stamp)) return false;
    seconds = stamp.unixtime();
    return true;
}

void printStatus(bool finished = false) {
    if (!queueMode) {
        if (role == Role::Hub) hub::printStats();
        else node::printStatus();
        return;
    }
    Serial.print(finished ? F("UTC END") : F("UTC STATUS"));
    Serial.print(F(" start=")); Serial.print(start);
    Serial.print(F(" mode=")); Serial.print(backlog::durable ? F("sd") : F("ram"));
    Serial.print(F(" trial=")); Serial.print(trialId);
    Serial.print(F(" cycles=")); Serial.print(min(cycle, count));
    Serial.print(F(" drain=")); Serial.print(cycle > count ? cycle - count : 0);
    Serial.print(F(" expected=")); Serial.print(count * Plan.nodes);
    Serial.print(F(" records=")); Serial.print(backlog::delivered);
    Serial.print(F(" accepted=")); Serial.print(backlog::saved);
    Serial.print(F(" backfilled=")); Serial.print(backlog::backfilled);
    Serial.print(F(" duplicates=")); Serial.print(backlog::duplicates);
    Serial.print(F(" ack_ms=")); Serial.print(ackTimeoutMs);
    Serial.print(F(" tx_dBm=")); Serial.print(powerDbm);
    if (role == Role::Node) {
        Serial.print(F(" pending=")); Serial.print((backlog::queueEnd - backlog::cursor) / backlog::JournalBytes);
        Serial.print(F(" confirmed=")); Serial.print(backlog::confirmed);
        Serial.print(F(" link_datagrams=")); Serial.print(backlog::datagramsAttempted);
        Serial.print(F(" link_datagrams_retried=")); Serial.print(backlog::datagramsRetried);
        Serial.print(F(" link_retries=")); Serial.print(backlog::radioRetries);
    }
    Serial.print(F(" fault=")); Serial.println(backlog::fault ? 1 : 0);
}

void stop(bool completed = false) {
    enabled = false;
    finished = completed;
    if (queueMode) {
        printStatus(true);
        radio.setTxPower(TxPowerDbm, false);
    } else if (role == Role::Hub && hub::running) hub::finishRun();
    if (role == Role::Node) radio.setModeIdle();
}

const __FlashStringHelper *arm(const Trial &trial, uint32_t &now) {
    const uint32_t nextSlot = trial.queue ? QueueSlotSeconds : SlotSeconds;
    const uint8_t id = role == Role::Hub ? hub::hubId : node::nodeId;
    if (enabled || (role == Role::Hub && (hub::running || hub::compareJsonNext))) {
        return F("UTC trial already running");
    }
    if (!ready || id == 0 ||
        (role == Role::Hub ? hub::enrollment : node::enrollment || id > Plan.nodes)) {
        return F("UTC slots need ready radios with fixed IDs");
    }
    LinkSettings queueLink = backlog::DefaultLink;
    queueLink.ackMs = trial.ack;
    const uint32_t required = trial.queue ? linkFrameMs(backlog::MaxBatchBytes, queueLink) + backlog::ReceiptWaitMs
                                          : slotMs(Plan);
    if (!queueLink.valid() || trial.power < 5 || trial.power > 23 || !trial.rounds ||
        Plan.nodes * nextSlot > trial.period || trial.period > 86400 ||
        required > 1000UL * (nextSlot - 2 * EdgeSeconds)) {
        return F("UTC trial settings or slot retry budget invalid");
    }
    if (!beginClock() || !readUtc(now) || trial.epoch <= now + BENCH_UTC_PREP_S ||
        uint64_t(trial.epoch) + (uint64_t(trial.rounds) + (trial.queue ? DrainCycles : 0)) * trial.period > LatestUtc) {
        return F("synchronize RTCs and choose a UTC start after the preparation window");
    }
    if (trial.queue && (!backlog::begin(Plan.nodes, trial.sd, trial.ack, trial.power, trial.epoch, trial.id) ||
                        !readUtc(now) || trial.epoch <= now + BENCH_UTC_PREP_S)) {
        radio.setTxPower(TxPowerDbm, false);
        return F("UTC queue setup failed; check SD, clock, trial ID and start time");
    }
    start = trial.epoch;
    count = trial.rounds;
    cycle = 0;
    queueMode = trial.queue;
    cycleSeconds = trial.period;
    slotSeconds = nextSlot;
    ackTimeoutMs = trial.ack;
    powerDbm = trial.power;
    trialId = trial.id;
    enabled = true;
    finished = false;
    remoteTrial = false;
    if (role == Role::Hub && !queueMode) {
        hub::Settings trial = hub::config;
        trial.plan = Plan;
        trial.source = Source;
        trial.format = Format;
        trial.padBytes = trial.gapMs = 0;
        trial.individual = trial.upload = false;
        trial.retries = Plan.link.retries;
        hub::beginRun(trial, count);
    } else if (role == Role::Node) {
        node::selectedHub = node::preferredHub;
        if (!trialId) radio.sleep();
    }
    return nullptr;
}

void printArmed(uint32_t now) {
    Serial.print(F("UTC start=")); Serial.print(start);
    Serial.print(F(" now=")); Serial.print(now);
    Serial.print(F(" cycle_s=")); Serial.print(cycleSeconds);
    Serial.print(F(" slot_s=")); Serial.print(slotSeconds);
    Serial.print(F(" nodes=")); Serial.print(Plan.nodes);
    Serial.print(F(" retries=")); Serial.println(Plan.link.retries);
}

void writeToken(uint8_t *frame, uint32_t epoch, uint8_t id) {
    write32(frame + 2, epoch);
    frame[6] = id;
}

bool matchesToken(const uint8_t *frame, const uint8_t *request) {
    return memcmp(frame + 2, request + 2, TokenBytes - 2) == 0;
}

bool handleRadio(const uint8_t *frame, uint8_t length, uint8_t senderHub, uint8_t destination) {
    if (role != Role::Node || length < 2 || frame[0] != NetworkId || frame[1] < RemoteArm ||
        frame[1] > RemoteResult) return false;
    if (node::enrollment || !node::nodeId || destination != node::nodeId ||
        senderHub != node::preferredHub || length < TokenBytes) return true;

    const uint32_t epoch = read32(frame + 2);
    const uint8_t id = frame[6];
    const bool sameTrial = remoteTrial && senderHub == remoteHub && epoch == start && id == trialId;
    if (frame[1] == RemoteArm && length == ArmBytes && id && frame[16] <= 1) {
        const Trial request{epoch, read16(frame + 7), read32(frame + 9), read16(frame + 13),
                            frame[15], id, true, frame[16] != 0};
        uint32_t now = 0;
        bool accepted = sameTrial && enabled && count == request.rounds && cycleSeconds == request.period &&
                        ackTimeoutMs == request.ack && powerDbm == request.power && backlog::durable == request.sd &&
                        readUtc(now);
        if (!sameTrial && !enabled) {
            // The hub can explicitly include its verified UTC in the arm request.
            const uint32_t hubTime = read32(frame + 17);
            const __FlashStringHelper *error = nullptr;
            if (hubTime) {
                if (hubTime < backlog::EarliestUtc || hubTime > LatestUtc ||
                    epoch <= hubTime + BENCH_UTC_PREP_S || !beginClock() ||
                    !loomTime::writeVerifiedUtc(clock, DateTime(hubTime), millis)) {
                    error = F("UTC clock sync failed");
                }
            }
            if (!error) error = arm(request, now);
            accepted = !error;
            if (error) {
                Serial.print(F("ERR ")); Serial.println(error);
            } else {
                remoteTrial = true;
                remoteHub = senderHub;
            }
        }
        uint8_t reply[ReadyBytes] = {NetworkId, RemoteReady};
        writeToken(reply, epoch, id);
        reply[7] = accepted;
        write32(reply + 8, now);
        // Control uses the boot power so a low-power test can still be armed and diagnosed.
        radio.setTxPower(TxPowerDbm, false);
        delay(PingTurnaroundMs);
        sendFrame(radio, hubAddress(senderHub), reply, sizeof(reply));
        if (enabled && queueMode) radio.setTxPower(powerDbm, false);
    } else if (length == TokenBytes && sameTrial) {
        if (frame[1] == RemoteStop && enabled) {
            stop();
        } else if (frame[1] == RemoteStats && !enabled) {
            uint8_t reply[ResultBytes] = {NetworkId, RemoteResult};
            writeToken(reply, start, trialId);
            write32(reply + 7, cycle);
            write32(reply + 11, backlog::datagramsAttempted);
            write32(reply + 15, backlog::datagramsRetried);
            write32(reply + 19, backlog::radioRetries);
            const uint32_t pending = (backlog::queueEnd - backlog::cursor) / backlog::JournalBytes;
            write32(reply + 23, pending);
            write32(reply + 27, backlog::confirmed);
            reply[31] = backlog::fault;
            reply[32] = finished;
            delay(PingTurnaroundMs);
            sendFrame(radio, hubAddress(senderHub), reply, sizeof(reply));
        }
    }
    return true;
}

bool requestNode(uint8_t id, const uint8_t *request, uint8_t length, uint8_t expectedKind,
                 uint8_t *reply, uint8_t replyBytes) {
    for (uint8_t attempt = 0; attempt < ControlAttempts; ++attempt) {
        if (!sendFrame(radio, id, request, length)) continue;
        const uint32_t sentAt = millis();
        while (millis() - sentAt < ControlWaitMs) {
            uint8_t size = replyBytes + 1; // Detect oversized frames as well as short frames.
            if (radio.recv(reply, &size) && size == replyBytes && radio.headerFrom() == id &&
                radio.headerTo() == hubAddress(hub::hubId) && reply[0] == NetworkId &&
                reply[1] == expectedKind && matchesToken(reply, request)) return true;
            delay(1);
        }
    }
    return false;
}

void stopStations() {
    uint8_t frame[TokenBytes] = {NetworkId, RemoteStop};
    writeToken(frame, start, trialId);
    radio.setTxPower(TxPowerDbm, false);
    for (uint8_t repeat = 0; repeat < ControlAttempts; ++repeat) {
        for (uint8_t id = 1; id <= Plan.nodes; ++id) sendFrame(radio, id, frame, sizeof(frame));
    }
}

bool armStations(const Trial &trial, bool syncClocks) {
    uint8_t frame[ArmBytes] = {NetworkId, RemoteArm};
    writeToken(frame, trial.epoch, trial.id);
    write16(frame + 7, static_cast<uint16_t>(trial.rounds));
    write32(frame + 9, trial.period);
    write16(frame + 13, trial.ack);
    frame[15] = trial.power;
    frame[16] = trial.sd;
    radio.setTxPower(TxPowerDbm, false);
    for (uint8_t id = 1; id <= Plan.nodes; ++id) {
        uint8_t reply[ReadyBytes + 1];
        uint32_t hubNow = 0;
        if (syncClocks && !readUtc(hubNow)) {
            Serial.println(F("ERR UTC hub RTC unavailable during arming"));
            return false;
        }
        write32(frame + 17, hubNow);
        const bool answered = requestNode(id, frame, sizeof(frame), RemoteReady, reply, ReadyBytes);
        const uint32_t nodeNow = answered ? read32(reply + 8) : 0;
        if (!answered || !reply[7] || !readUtc(hubNow) ||
            nodeNow > hubNow + 1 || hubNow > nodeNow + 1 ||
            start <= hubNow + BENCH_UTC_PREP_S) {
            Serial.print(F("ERR UTC arm failed at node=")); Serial.print(id);
            Serial.println(F(" (check radio, RTC, SD card and trial ID)"));
            return false;
        }
        Serial.print(F("UTC READY node=")); Serial.println(id);
    }
    radio.setTxPower(powerDbm, false);
    return true;
}

void collectStationResults() {
    uint8_t frame[TokenBytes] = {NetworkId, RemoteStats};
    writeToken(frame, start, trialId);
    // Nodes reach the end after their own final slot. Leave one clock tick of margin.
    delay(1100);
    radio.setTxPower(TxPowerDbm, false);
    for (uint8_t id = 1; id <= Plan.nodes; ++id) {
        uint8_t reply[ResultBytes + 1];
        const bool answered = requestNode(id, frame, sizeof(frame), RemoteResult, reply, ResultBytes);
        Serial.print(F("UTC NODE start=")); Serial.print(start);
        Serial.print(F(" trial=")); Serial.print(trialId);
        Serial.print(F(" node=")); Serial.print(id);
        Serial.print(F(" report=")); Serial.print(answered ? 1 : 0);
        if (answered) {
            const uint32_t completed = read32(reply + 7);
            Serial.print(F(" cycles=")); Serial.print(min(completed, count));
            Serial.print(F(" drain=")); Serial.print(completed > count ? completed - count : 0);
            Serial.print(F(" link_datagrams=")); Serial.print(read32(reply + 11));
            Serial.print(F(" link_datagrams_retried=")); Serial.print(read32(reply + 15));
            Serial.print(F(" link_retries=")); Serial.print(read32(reply + 19));
            Serial.print(F(" pending=")); Serial.print(read32(reply + 23));
            Serial.print(F(" confirmed=")); Serial.print(read32(reply + 27));
            Serial.print(F(" fault=")); Serial.print(reply[31]);
            Serial.print(F(" finished=")); Serial.print(reply[32]);
        }
        Serial.println();
    }
}

bool handleCommand(char *line) {
    char *verb = line + strspn(line, " \t");
    if (strncmp(verb, "utc", 3) || (verb[3] && verb[3] != ' ' && verb[3] != '\t')) return false;
    const char *epoch = strtok(verb + 3, " \t");
    const bool remote = epoch && !strcmp(epoch, "radio");
    if (remote) epoch = strtok(nullptr, " \t");
    const bool useSD = epoch && !strcmp(epoch, "sd");
    const bool useQueue = useSD || (epoch && !strcmp(epoch, "ram"));
    if (useQueue) epoch = strtok(nullptr, " \t");
    const char *rounds = strtok(nullptr, " \t");
    if (!remote && !useQueue && epoch && !strcmp(epoch, "clock")) {
        uint32_t now = 0;
        if (enabled || (role == Role::Hub && hub::running) ||
            !parseNumber(rounds, backlog::EarliestUtc, LatestUtc, now) || strtok(nullptr, " \t") ||
            !beginClock() || !loomTime::writeVerifiedUtc(clock, DateTime(now), millis) || !readUtc(now)) {
            Serial.println(F("ERR use utc clock <Unix UTC seconds> on an idle board with a working RTC"));
        } else {
            Serial.print(F("UTC CLOCK now=")); Serial.println(now);
        }
        return true;
    }
    if (!remote && !useQueue && epoch && !rounds) {
        if (!strcmp(epoch, "stop")) {
            if (enabled) {
                if (remoteTrial && role == Role::Hub) stopStations();
                stop();
            }
            return true;
        }
        if (!strcmp(epoch, "status")) {
            printStatus();
            return true;
        }
    }
    uint32_t now = 0, nextStart, nextCount;
    uint32_t nextTimeout = DefaultAckTimeoutMs, nextPower = TxPowerDbm;
    uint32_t nextCycle = CycleSeconds, nextTrial = 0;
    const char *ack = useQueue ? strtok(nullptr, " \t") : nullptr;
    const char *power = ack ? strtok(nullptr, " \t") : nullptr;
    const char *period = power ? strtok(nullptr, " \t") : nullptr;
    const char *idArg = period ? strtok(nullptr, " \t") : nullptr;
    const char *syncArg = idArg && remote ? strtok(nullptr, " \t") : nullptr;
    const bool syncClocks = syncArg && !strcmp(syncArg, "sync");
    if ((remote && (role != Role::Hub || !useQueue || !idArg)) ||
        (syncArg && !syncClocks) ||
        !parseNumber(epoch, backlog::EarliestUtc, LatestUtc, nextStart) ||
        !parseNumber(rounds, 1, 65535, nextCount) ||
        (ack && !parseNumber(ack, 60, 1000, nextTimeout)) ||
        (power && !parseNumber(power, 5, 23, nextPower)) ||
        (period && !parseNumber(period, 1, 86400, nextCycle)) ||
        (idArg && !parseNumber(idArg, remote ? 1 : 0, 255, nextTrial)) || strtok(nullptr, " \t")) {
        Serial.println(F("ERR use utc [radio] [sd|ram] <epoch> <rounds> [ack_ms tx_dBm cycle_s trial]"));
        return true;
    }
    const Trial trial{nextStart, nextCount, nextCycle, static_cast<uint16_t>(nextTimeout),
                      static_cast<uint8_t>(nextPower), static_cast<uint8_t>(nextTrial), useQueue, useSD};
    if (const __FlashStringHelper *error = arm(trial, now)) {
        Serial.print(F("ERR ")); Serial.println(error);
        return true;
    }
    if (remote) {
        remoteTrial = true;
        const bool armed = armStations(trial, syncClocks);
        if (!armed || !readUtc(now)) {
            if (armed) Serial.println(F("ERR UTC hub RTC unavailable after arming"));
            stopStations();
            stop();
            return true;
        }
    }
    printArmed(now);
    return true;
}

void listenForControl() {
    if (!remoteTrial || role != Role::Node) return;
    uint8_t frame[ArmBytes + 1];
    uint8_t length = sizeof(frame);
    if (radio.recv(frame, &length)) {
        handleRadio(frame, length, hubIdFromAddress(radio.headerFrom()), radio.headerTo());
    }
}

void service() {
    if (queueMode && backlog::fault) {
        stop();
        return;
    }
    if (cycle == count + (queueMode ? DrainCycles : 0)) {
        if (remoteTrial && role == Role::Hub) collectStationResults();
        stop(true);
        return;
    }
    uint32_t now;
    if (!readUtc(now)) {
        Serial.println(F("ERR RTC lost valid UTC during the trial"));
        if (remoteTrial && role == Role::Hub) stopStations();
        stop();
        return;
    }
    const uint32_t groupStart = start + cycle * cycleSeconds;
    if (role == Role::Hub) {
        if (now < groupStart) {
            delay(10);
            return;
        }
        const uint32_t receivedAt = millis();
        const uint32_t windowStart = receivedAt - (now - groupStart) * 1000UL;
        if (queueMode) {
            backlog::receiveQueued(groupStart, windowStart, slotSeconds * 1000UL, Plan.nodes);
            ++cycle;
            return;
        }
        if (!cycle) hub::runStarted = windowStart;
        const SlotMask due = slotMask(Plan.nodes);
        memset(hub::received, 0, sizeof(hub::received));
        hub::roundRxFrameBytes = 0;
        hub::receiveRound(static_cast<uint16_t>(cycle), windowStart, due, 0, false, slotSeconds * 1000UL);
        const SlotMask missing = hub::recordFirstPass(due);
        hub::radioMs += millis() - receivedAt;
        hub::maxRoundMs = max(hub::maxRoundMs, millis() - receivedAt);
        hub::maxPollFinishMs = hub::maxRoundMs;
        hub::processRound(missing, due, ++hub::rounds);
    } else {
        listenForControl();
        if (!enabled) return;
        const uint32_t slot = groupStart + (node::nodeId - 1) * slotSeconds;
        const uint32_t prepareAt = slot - BENCH_UTC_PREP_S;
        if (now < prepareAt) {
            const uint32_t seconds = min(30UL, prepareAt - now);
            // Recheck the external UTC clock after each MCU wake. The RFM95 stays asleep.
            if (seconds > 1 && !trialId) {
                LoomWatchdogPause pause;
                LowPower.sleep((seconds - 1) * 1000UL);
            } else {
                delay(50);
            }
            return;
        }
        if (queueMode && now < groupStart + cycleSeconds) {
            backlog::sendQueued(groupStart, slot, now, slotSeconds, EdgeSeconds, cycle < count);
            if (!trialId) radio.sleep();
        } else if (!queueMode && now < slot) {
            SampleRequest request{Plan, static_cast<uint16_t>(cycle), 0, Format, Source};
            request.plan.leadMs = (slot - now + EdgeSeconds) * 1000UL;
            uint8_t frame[RH_RF95_MAX_MESSAGE_LEN];
            node::respondToPoll(frame, request, millis(), 1, 1, node::ResponseMode::Slots);
            radio.sleep();
        } else {
            ++node::skipped;
        }
    }
    ++cycle;
}

} // namespace utcSlots
} // namespace bench
