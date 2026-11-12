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

void stop() {
    enabled = false;
    if (queueMode) {
        printStatus(true);
        radio.setTxPower(TxPowerDbm, false);
    } else if (role == Role::Hub && hub::running) hub::finishRun();
    if (role == Role::Node) radio.setModeIdle();
}

bool handleCommand(char *line) {
    char *verb = line + strspn(line, " \t");
    if (strncmp(verb, "utc", 3) || (verb[3] && verb[3] != ' ' && verb[3] != '\t')) return false;
    const char *epoch = strtok(verb + 3, " \t");
    const bool useSD = epoch && !strcmp(epoch, "sd");
    const bool useQueue = useSD || (epoch && !strcmp(epoch, "ram"));
    if (useQueue) epoch = strtok(nullptr, " \t");
    const char *rounds = strtok(nullptr, " \t");
    if (!useQueue && epoch && !strcmp(epoch, "stop") && !rounds) {
        if (enabled) stop();
        return true;
    }
    if (!useQueue && epoch && !strcmp(epoch, "status") && !rounds) {
        printStatus();
        return true;
    }
    uint32_t now = 0, nextStart, nextCount;
    uint32_t nextTimeout = DefaultAckTimeoutMs, nextPower = TxPowerDbm;
    uint32_t nextCycle = CycleSeconds, nextTrial = 0;
    const uint32_t nextSlot = useQueue ? QueueSlotSeconds : SlotSeconds;
    const char *ack = useQueue ? strtok(nullptr, " \t") : nullptr;
    const char *power = ack ? strtok(nullptr, " \t") : nullptr;
    const char *period = power ? strtok(nullptr, " \t") : nullptr;
    const char *idArg = period ? strtok(nullptr, " \t") : nullptr;
    if (enabled || (role == Role::Hub && (hub::running || hub::compareJsonNext)) ||
        !parseNumber(epoch, backlog::EarliestUtc, 4102444799UL, nextStart) ||
        !parseNumber(rounds, 1, 65535, nextCount) ||
        (ack && !parseNumber(ack, 60, 1000, nextTimeout)) ||
        (power && !parseNumber(power, 5, 23, nextPower)) ||
        (period && !parseNumber(period, 1, 86400, nextCycle)) ||
        (idArg && !parseNumber(idArg, 0, 255, nextTrial)) || strtok(nullptr, " \t")) {
        Serial.println(F("ERR use utc [sd|ram] <epoch> <rounds> [ack_ms tx_dBm cycle_s trial] or utc stop"));
        return true;
    }
    const uint8_t id = role == Role::Hub ? hub::hubId : node::nodeId;
    if (!ready || id == 0 ||
        (role == Role::Hub ? hub::enrollment : node::enrollment || id > Plan.nodes)) {
        Serial.println(F("ERR UTC slots need ready radios with fixed IDs"));
        return true;
    }
    LinkSettings queueLink = backlog::DefaultLink;
    queueLink.ackMs = static_cast<uint16_t>(nextTimeout);
    const uint32_t required = useQueue ? linkFrameMs(backlog::MaxBatchBytes, queueLink) + backlog::ReceiptWaitMs
                                       : slotMs(Plan);
    if (Plan.nodes * nextSlot > nextCycle || required > 1000UL * (nextSlot - 2 * EdgeSeconds)) {
        Serial.println(F("ERR UTC slot retry budget or cycle length does not fit"));
        return true;
    }
    if (!clock.begin() || !readUtc(now) || nextStart <= now + BENCH_UTC_PREP_S ||
        uint64_t(nextStart) + (uint64_t(nextCount) + (useQueue ? DrainCycles : 0)) * nextCycle > 4102444799ULL) {
        Serial.println(F("ERR synchronize RTCs and choose a UTC start after the preparation window"));
        return true;
    }
    if (useQueue && (!backlog::begin(Plan.nodes, useSD, static_cast<uint16_t>(nextTimeout),
                                   static_cast<uint8_t>(nextPower), nextStart, static_cast<uint8_t>(nextTrial)) ||
                     !readUtc(now) || nextStart <= now + BENCH_UTC_PREP_S)) {
        radio.setTxPower(TxPowerDbm, false);
        Serial.println(F("ERR UTC queue setup failed; check SD, clock and start time"));
        return true;
    }
    start = nextStart;
    count = nextCount;
    cycle = 0;
    queueMode = useQueue;
    cycleSeconds = nextCycle;
    slotSeconds = nextSlot;
    ackTimeoutMs = nextTimeout;
    powerDbm = nextPower;
    trialId = nextTrial;
    enabled = true;
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
    Serial.print(F("UTC start=")); Serial.print(start);
    Serial.print(F(" now=")); Serial.print(now);
    Serial.print(F(" cycle_s=")); Serial.print(cycleSeconds);
    Serial.print(F(" slot_s=")); Serial.print(slotSeconds);
    Serial.print(F(" nodes=")); Serial.print(Plan.nodes);
    Serial.print(F(" retries=")); Serial.println(Plan.link.retries);
    return true;
}

void service() {
    if (cycle == count + (queueMode ? DrainCycles : 0) || (queueMode && backlog::fault)) {
        stop();
        return;
    }
    uint32_t now;
    if (!readUtc(now)) {
        Serial.println(F("ERR RTC lost valid UTC during the trial"));
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
