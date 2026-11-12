#pragma once

#include <Hardware/Loom_Hypnos/SDManager.h>

#ifndef BENCH_SD_CS_PIN
#define BENCH_SD_CS_PIN 11 // Hypnos V3.3. V3.2 uses D10.
#endif

namespace bench {
namespace sweep {

constexpr uint32_t PollsPerTest = 1000, SizingTimeoutMs = 300000, ProductionPeriodMs = 300000;

struct Profile {
    uint16_t leadMs, extraMs;
    uint8_t headroom, retries;
};

// A positive extraMs is added to the fitted first-pass window to obtain a fixed poll period.
// Zero selects back-to-back polls. Every profile runs synthetic and live sensor traffic.
constexpr Profile Profiles[] = {
    {120, 0, 16, 0}, {120, 0, 16, MaxRetries}, {120, 500, 16, MaxRetries}, {120, 2000, 16, MaxRetries},
    {250, 500, 16, MaxRetries}, {500, 500, 32, MaxRetries}, {120, 2000, 16, 1}, {120, 2000, 16, 0}
};
constexpr uint8_t ProfileCount = sizeof(Profiles) / sizeof(Profiles[0]);

enum class Stage : uint8_t { Idle, Sizing, Measuring };
Stage stage = Stage::Idle;
SDManager storage(&hub::manager, BENCH_SD_CS_PIN);
hub::Settings savedSettings;
uint32_t savedPeriods[MaxNodes];
uint32_t startedAt = 0, durationMs = 0, sizingStarted = 0, periodMs = 0, testNumber = 0;
uint8_t profileIndex = 0, source = SyntheticSample, repetition = 1;
char reportPath[13] = {}, csvPath[13] = {};

struct Result {
    uint32_t requested = 0, delivered = 0, empty = 0, skipped = 0, firstLoss = 0;
    uint32_t recovered = 0, fields = 0, expectedFields = 0;
    bool clean = false;
};

struct Best {
    bool present = false;
    hub::Settings settings{};
    uint8_t profile = 0;
    uint32_t confirmedAtTest = 0;
    uint32_t periodMs = 0, retrySlots = 0, finishMs = 0;
    float samplesPerMinute = 0;
};

Best fastest[2], fewestRetries[2];
uint32_t firstRetrySlots = 0, firstFinishMs = 0, firstWallMs = 0;
Result result;
const char *recordStatus = "";
bool recordHasResult = false;
bool logToSD = true;

class CheckedOutput : public Print {
  public:
    explicit CheckedOutput(Print &target) : target(target) {}
    using Print::write;
    size_t write(uint8_t value) override { return write(&value, 1); }
    size_t write(const uint8_t *data, size_t size) override {
        if (getWriteError()) return 0;
        const size_t written = target.write(data, size);
        if (written != size) setWriteError(1);
        return written;
    }
  private:
    Print &target;
};

bool isActive() { return stage != Stage::Idle; }

void restore() {
    hub::config = savedSettings;
    for (uint8_t i = 0; i < MaxNodes; ++i) hub::uploadPeriods[i] = savedPeriods[i];
    stage = Stage::Idle;
}

void printBest(Print &output, const char *label, uint8_t kind, const Best &best) {
    output.print(label); output.print(kind == SensorSample ? F(" sensors") : F(" synthetic"));
    if (!best.present) {
        output.println(F(" no qualifying pair yet"));
        return;
    }
    output.print(F(" profile=")); output.print(best.profile);
    output.print(F(" confirmed_at_test=")); output.print(best.confirmedAtTest);
    output.print(F(" samples_per_min=")); output.print(best.samplesPerMinute);
    output.print(F(" period_ms=")); output.print(best.periodMs);
    output.print(F(" retry_slots_in_two_tests=")); output.print(best.retrySlots);
    output.print(F(" poll_finish_max_ms=")); output.print(best.finishMs);
    output.print(F(" estimated_spare_ms="));
    output.println(best.finishMs < ProductionPeriodMs ? ProductionPeriodMs - best.finishMs : 0);
    hub::printSettings(best.settings, output);
}

void printRecommendations(Print &output) {
    for (uint8_t kind = SensorSample; kind <= SyntheticSample; ++kind) {
        printBest(output, "FASTEST", kind, fastest[kind]);
        printBest(output, "FEWEST_RETRIES", kind, fewestRetries[kind]);
    }
}

bool writeReport(Print &raw, void *) {
    CheckedOutput output(raw);
    output.print(F("SWEEP status=")); output.print(recordStatus);
    output.print(F(" uptime_ms=")); output.print(millis());
    output.print(F(" test=")); output.print(testNumber);
    output.print(F(" profile=")); output.print(profileIndex + 1);
    output.print(F(" source=")); output.print(source == SensorSample ? F("sensors") : F("synthetic"));
    output.print(F(" repeat=")); output.print(repetition);
    output.print(F(" period_ms=")); output.print(periodMs);
    output.print(F(" duration_ms=")); output.print(durationMs);
    output.print(F(" nodes=")); output.print(savedSettings.plan.nodes);
    output.print(F(" serial=")); output.println(hub::manager.get_serial_num());
    if (recordHasResult) hub::printStats(output);
    printRecommendations(output);
    output.println();
    return !output.getWriteError();
}

bool writeCsv(Print &raw, void *) {
    CheckedOutput output(raw);
    if (!recordHasResult) {
        output.println(F("test,profile,source,repeat,status,nodes,period_ms,lead_ms,slot_ms,chunk_B,fragments,retry_limit,polls,requested,delivered,empty,first_loss,recovered,retry_slots,beacon_failures,retry_beacon_failures,skipped_deadlines,populated_fields,expected_fields,wall_ms,radio_ms,round_max_ms,poll_finish_max_ms,clean"));
        return !output.getWriteError();
    }
    const hub::Settings &s = hub::active;
    output.print(testNumber); output.print(','); output.print(profileIndex + 1); output.print(',');
    output.print(source == SensorSample ? F("sensors") : F("synthetic")); output.print(',');
    output.print(repetition); output.print(','); output.print(recordStatus); output.print(',');
    const uint32_t values[] = {
        s.plan.nodes, periodMs, s.plan.leadMs, slotMs(s.plan), s.plan.chunk, s.plan.fragments,
        s.retries, hub::rounds + hub::beaconFailures, result.requested, result.delivered,
        result.empty, result.firstLoss, result.recovered, hub::retrySlots, hub::beaconFailures,
        hub::retryBeaconFailures, result.skipped, result.fields, result.expectedFields,
        hub::runElapsed, hub::radioMs, hub::maxRoundMs, hub::maxPollFinishMs
    };
    for (uint32_t value : values) { output.print(value); output.print(','); }
    output.println(result.clean ? 1 : 0);
    return !output.getWriteError();
}

bool saveRecord(const char *status, bool hasResult) {
    recordStatus = status;
    recordHasResult = hasResult;
    if (!logToSD) { writeReport(Serial, nullptr); return true; }
    if (storage.writeDebugRecords(reportPath, writeReport, nullptr) &&
        (!hasResult || storage.writeDebugRecords(csvPath, writeCsv, nullptr))) return true;
    Serial.println(F("ERR sweep SD write failed. Sweep stopped; check the saved session files."));
    hub::deactivate();
    restore();
    return false;
}

void captureResult(bool complete) {
    result = {};
    result.requested = PollsPerTest * hub::active.plan.nodes;
    result.fields = hub::volume.populatedFields;
    bool allNodes = true;
    for (uint8_t i = 0; i < hub::active.plan.nodes; ++i) {
        const hub::NodeStats &s = hub::stats[i];
        result.delivered += s.good - s.empty;
        result.empty += s.empty;
        result.skipped += s.skippedDeadlines;
        result.firstLoss += s.firstMissing + s.firstPartial;
        result.recovered += s.recovered;
        allNodes = allNodes && s.good == PollsPerTest && s.empty == 0;
    }
    result.expectedFields = result.delivered * (source == SensorSample ? 7 : 2);
    result.clean = complete && allNodes && !result.skipped && !hub::beaconFailures &&
                   result.fields == result.expectedFields &&
                   (!periodMs || hub::maxPollFinishMs <= periodMs);
}

void considerRecommendations() {
    Best candidate;
    candidate.present = true;
    candidate.settings = hub::active;
    candidate.profile = profileIndex + 1;
    candidate.confirmedAtTest = testNumber;
    candidate.periodMs = periodMs;
    candidate.retrySlots = firstRetrySlots + hub::retrySlots;
    candidate.finishMs = max(firstFinishMs, hub::maxPollFinishMs);
    const uint32_t wall = firstWallMs + hub::runElapsed;
    candidate.samplesPerMinute = wall ? 120000.0f * result.requested / wall : 0;
    if (!fastest[source].present || candidate.samplesPerMinute > fastest[source].samplesPerMinute)
        fastest[source] = candidate;
    const Best &best = fewestRetries[source];
    if (!best.present || candidate.retrySlots < best.retrySlots ||
        (candidate.retrySlots == best.retrySlots && candidate.samplesPerMinute > best.samplesPerMinute))
        fewestRetries[source] = candidate;
}

void stop(const char *reason) {
    if (!isActive()) return;
    const bool measuring = stage == Stage::Measuring;
    const bool complete = measuring && !hub::running &&
        hub::rounds + hub::beaconFailures == PollsPerTest;
    hub::deactivate();
    if (measuring) {
        captureResult(complete);
        if (result.clean && repetition == 2) considerRecommendations();
    }
    if (!saveRecord(reason, measuring)) return;
    printRecommendations(Serial);
    Serial.print(F("SWEEP stopped: ")); Serial.println(reason);
    restore();
}

void beginSizing() {
    hub::config = savedSettings;
    const Profile &profile = Profiles[profileIndex];
    hub::config.plan.link = hub::DefaultPlan.link;
    hub::config.plan.leadMs = profile.leadMs;
    hub::config.padBytes = hub::config.gapMs = 0;
    hub::config.format = MsgPack;
    hub::config.source = source;
    hub::config.individual = false;
    hub::config.upload = false;
    hub::config.retries = profile.retries;
    for (uint32_t &period : hub::uploadPeriods) period = 0;
    repetition = 1;
    periodMs = 0;
    stage = Stage::Sizing;
    sizingStarted = millis();
    hub::armRun(1);
}

void nextCase() {
    if (source == SensorSample && profileIndex + 1 == ProfileCount) {
        stage = Stage::Sizing;
        stop("suite_complete");
        return;
    }
    if (source == SyntheticSample) {
        source = SensorSample;
    } else {
        source = SyntheticSample;
        ++profileIndex;
    }
    beginSizing();
}

void beginMeasurement() {
    ++testNumber;
    stage = Stage::Measuring;
    if (!saveRecord("test_started", false)) return;
    hub::beginRun(hub::config, PollsPerTest, true);
}

void service() {
    if (!isActive()) return;
    if (millis() - startedAt >= durationMs) { stop("time_limit"); return; }
    if (stage == Stage::Sizing) {
        if (hub::running && millis() - sizingStarted < SizingTimeoutMs) return;
        if (hub::running) hub::deactivate();
        if (!hub::fitSlots(Profiles[profileIndex].headroom)) {
            if (saveRecord("sizing_failed", false)) nextCase();
            return;
        }
        const Profile &profile = Profiles[profileIndex];
        if (profile.extraMs) {
            const uint8_t beaconBytes = BeaconBytes + SlotMaskBytes +
                (hub::enrollment ? hub::config.plan.nodes * BoardKeyBytes : 0);
            const uint32_t firstPass = airtimeMs(beaconBytes) + hub::config.plan.leadMs +
                hub::config.plan.nodes * slotMs(hub::config.plan);
            periodMs = ((firstPass + profile.extraMs + 99) / 100) * 100;
        }
        for (uint32_t &period : hub::uploadPeriods) period = periodMs;
        beginMeasurement();
        return;
    }
    if (hub::running) return;
    captureResult(true);
    if (result.clean && repetition == 2) considerRecommendations();
    if (!saveRecord(result.clean ? "clean" : "failed", true)) return;
    if (result.clean && repetition == 1) {
        firstRetrySlots = hub::retrySlots;
        firstFinishMs = hub::maxPollFinishMs;
        firstWallMs = hub::runElapsed;
        repetition = 2;
        beginMeasurement();
    } else {
        nextCase();
    }
}

bool start(uint32_t hours, bool saveToSD = true) {
    if (role != Role::Hub || !ready || hub::running || hub::compareJsonNext || isActive()) {
        Serial.println(F("ERR sweep start requires an idle, ready hub"));
        return false;
    }
    if (hours < 1 || hours > 72) return false;
    savedSettings = hub::config;
    for (uint8_t i = 0; i < MaxNodes; ++i) savedPeriods[i] = hub::uploadPeriods[i];
    logToSD = saveToSD;
    if (logToSD) {
#if BENCH_ENABLE_DENDROMETER
        node::dendrometer.setPower(true);
#endif
        radio.setModeIdle();
        if (!storage.begin()) {
            Serial.println(F("ERR sweep needs a writable SD card"));
            return false;
        }
        bool found = false;
        for (uint16_t file = 1; file <= 9999; ++file) {
            snprintf(reportPath, sizeof(reportPath), "SW%04u.TXT", static_cast<unsigned>(file));
            snprintf(csvPath, sizeof(csvPath), "SW%04u.CSV", static_cast<unsigned>(file));
            if (!storage.fileExists(reportPath) && !storage.fileExists(csvPath)) { found = true; break; }
        }
        if (!found) { Serial.println(F("ERR sweep session filenames exhausted")); return false; }
    } else {
        reportPath[0] = csvPath[0] = '\0';
    }
    startedAt = millis();
    durationMs = hours * 3600000UL;
    profileIndex = 0;
    source = SyntheticSample;
    repetition = 1;
    periodMs = testNumber = 0;
    recordHasResult = false;
    for (uint8_t kind = 0; kind < 2; ++kind) { fastest[kind] = {}; fewestRetries[kind] = {}; }
    if ((logToSD && !storage.writeDebugRecords(csvPath, writeCsv, nullptr)) ||
        !saveRecord("session_started", false)) {
        Serial.println(F("ERR sweep could not save the session header"));
        return false;
    }
    if (logToSD) {
        Serial.print(F("SWEEP started. SD results: ")); Serial.print(csvPath);
        Serial.print(F(" and ")); Serial.println(reportPath);
    } else {
        Serial.println(F("SWEEP started. Capture USB serial output for the results."));
    }
    beginSizing();
    return true;
}

bool handleCommand(char *line) {
    const char *begin = line + strspn(line, " \t");
    char verb[16] = {};
    if (sscanf(begin, "%15s", verb) != 1) return false;
    if (!strcmp(verb, "sweep")) {
        strtok(line, " \t");
        const char *action = strtok(nullptr, " \t");
        const char *arg = strtok(nullptr, " \t");
        uint32_t hours = 8;
        if (strtok(nullptr, " \t")) {
            Serial.println(F("ERR use sweep start|usb [1-72 hours], sweep status, or sweep stop"));
        } else if (action && (!strcmp(action, "start") || !strcmp(action, "usb")) &&
                   (!arg || parseNumber(arg, 1, 72, hours))) {
            start(hours, !strcmp(action, "start"));
        } else if (action && !arg && !strcmp(action, "stop")) {
            stop("user_stop");
        } else if (action && !arg && !strcmp(action, "status")) {
            Serial.print(F("SWEEP active=")); Serial.print(isActive() ? F("yes") : F("no"));
            Serial.print(F(" profile=")); Serial.print(profileIndex + 1);
            Serial.print(F(" test=")); Serial.print(testNumber);
            Serial.print(F(" report=")); Serial.println(reportPath);
            printRecommendations(Serial);
        } else {
            Serial.println(F("ERR use sweep start|usb [1-72 hours], sweep status, or sweep stop"));
        }
        return true;
    }
    if (!isActive()) return false;
    if (!strcmp(verb, "stop")) { stop("user_stop"); return true; }
    if (!strcmp(verb, "status") || !strcmp(verb, "stats") || !strcmp(verb, "sample") ||
        !strcmp(verb, "help") || !strcmp(verb, "role")) return false;
    Serial.println(F("ERR sweep is active. Use sweep stop before changing settings."));
    return true;
}

} // namespace sweep
} // namespace bench
