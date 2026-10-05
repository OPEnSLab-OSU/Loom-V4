// Full debug sketch: phase-by-phase memory checks and optional trace/heap capture.
// Start with the matching _debug_minimal sketch for a simple flag-only baseline.
// See DEBUG-GUIDE.md for what each layer adds and how to compare builds.

// START HERE: choose the evidence you want, then rebuild.
// TRACE FILES = when calls ran + how RAM changed; inspect after the run.
// DEBUG TEXT = what the device is doing + why a step failed; read while running.
// Both can be ON together. Trace does not require DEBUG text or its SD text copy.
// Trace: /debug/trace_N.perfetto.json + trace_N.ndjson. Text: output_N.log if enabled.
// WARNING/ERROR remain visible with DEBUG text OFF. See DEBUG-GUIDE.md for recipes.

// TEXT: progress and failure explanations in Serial Monitor.
// 0 hides routine DEBUG messages/JSON; WARNING and ERROR still print.
#ifndef LOOM_DEBUG_TEXT
#define LOOM_DEBUG_TEXT 1
#endif
// Copy Logger messages to /debug/output_N.log (including warnings/errors).
// Direct Serial memory/mux/SD reports are not copied by this switch.
#ifndef LOOM_DEBUG_SD_LOG
#define LOOM_DEBUG_SD_LOG 1
#endif

// BEGIN LOOM_TRACE_DIAGNOSTICS
// TRACE FILES: 0/0 = off; 1/0 = call timeline + RAM totals; 1/1 = allocator windows too.
// Heap capture needs Loom_TraceHeap for IDE builds; the launcher can supply the hooks.
#ifndef LOOM_TRACE
#define LOOM_TRACE 1
#endif
#ifndef LOOM_TRACE_HEAP
#define LOOM_TRACE_HEAP 1
#endif
// Bound allocation bursts without writing SD from allocator hooks. Range: 1..23.
// Set LOOM_TRACE_HEAP to 0 for calls, objects and heap/free-RAM totals only.
#ifndef LOOM_TRACE_HEAP_WINDOW_EVENTS
#define LOOM_TRACE_HEAP_WINDOW_EVENTS 16
#endif
// END LOOM_TRACE_DIAGNOSTICS

// BEGIN LOOM_BETA_DIAGNOSTICS
// EXTRA TEXT REPORTS: memory, discovery and SD decisions during a bench run.
// This default controls the detailed reports below, not TRACE or routine DEBUG text.
#ifndef LOOM_DEBUG_DIAGNOSTICS
#ifdef LOOM_WISP_BETA_DIAGNOSTICS
#define LOOM_DEBUG_DIAGNOSTICS LOOM_WISP_BETA_DIAGNOSTICS
#else
#define LOOM_DEBUG_DIAGNOSTICS 1
#endif
#endif
// Serial allocator/stack estimates, deltas, JSON use/overflow and batch count.
// This measures existing allocator state; it does not probe by allocating blocks.
#ifndef LOOM_DEBUG_MEMORY
#define LOOM_DEBUG_MEMORY LOOM_DEBUG_DIAGNOSTICS
#endif
// Extra Serial report: SD write decisions/results.
#ifndef LOOM_DEBUG_SD_WRITES
#define LOOM_DEBUG_SD_WRITES LOOM_DEBUG_DIAGNOSTICS
#endif
// Verbose sensor discovery/scan logs; does not select drivers or change wiring.
#ifndef LOOM_DEBUG_MUX_SCAN
#define LOOM_DEBUG_MUX_SCAN LOOM_DEBUG_DIAGNOSTICS
#endif
// Pretty-print sensor JSON through Logger; needs LOOM_DEBUG_TEXT=1 too.
#ifndef LOOM_DEBUG_PRINT_SAMPLES
#define LOOM_DEBUG_PRINT_SAMPLES 1
#endif

// END LOOM_BETA_DIAGNOSTICS

// Discover Loom before its optional diagnostic headers (required by Arduino IDE).
#include <Loom_Manager.h>
#include <Diagnostics/Loom_TraceSketch.h>
#if LOOM_DEBUG_MEMORY
#include <Diagnostics/Loom_MemoryDiagnostics.h>
#endif
#include <Hardware/Loom_BatchSD/Loom_BatchSD.h>

#include <Adafruit_SleepyDog.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
// Optional: delete this entire three-line block to bundle every supported mux driver.
// The runtime scan addresses below stay independent, so deleting this block still compiles.
#ifndef LOOM_MUX_COMPILED_ADDRESSES
#define LOOM_MUX_COMPILED_ADDRESSES 0x74, 0x15, 0x6B, 0x44
#endif
#include <Hardware/Loom_Multiplexer/Loom_Multiplexer.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Logger.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Utilities/Loom_TimeUtils.h>

constexpr int ACTIVE_WATCHDOG_MS = 16000;

void enableActiveWatchdog() {
    Watchdog.enable(ACTIVE_WATCHDOG_MS);
    Watchdog.reset();
}

Manager manager("Deploy_Test_", 8);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

Loom_LTE lte(manager, "hologram", "", "");
Loom_MongoDB mqtt(manager, lte);
// Publish every 72 records. This bench schedule varies wall-clock time between records.
Loom_BatchSD batchSD(hypnos, 72);

// BEGIN LOOM_BETA_DIAGNOSTICS
// One named checkpoint serves both kinds of evidence.
// TRACE gets a RAM snapshot in the files; MEMORY gets detailed live Serial estimates.
// Each output follows its own flag, so the same call works for either or both.
#if LOOM_DEBUG_MEMORY
Loom_MemoryDiagnostics memoryDiagnostics;
#define WISP_DIAGNOSTIC_BEGIN_CYCLE() memoryDiagnostics.beginCycle()
#define WISP_SERIAL_MEMORY_CHECKPOINT(phaseLabel) \
    memoryDiagnostics.checkpoint(F(phaseLabel), manager.getDocument(), batchSD.getCurrentBatch())
#else
#define WISP_DIAGNOSTIC_BEGIN_CYCLE() do {} while (false)
#define WISP_SERIAL_MEMORY_CHECKPOINT(phaseLabel) do {} while (false)
#endif
#define WISP_DIAGNOSTIC_CHECKPOINT(phaseLabel) \
    do { \
        WISP_SERIAL_MEMORY_CHECKPOINT(phaseLabel); \
        LOOM_TRACE_CHECKPOINT(phaseLabel); \
    } while (false)
#if LOOM_DEBUG_MUX_SCAN
#define WISP_DIAGNOSTIC_ENABLE_MUX_SCAN() \
    do { mux.setDebug(true); mux.setScanDebug(true); } while (false)
#else
#define WISP_DIAGNOSTIC_ENABLE_MUX_SCAN() do {} while (false)
#endif
#if LOOM_DEBUG_SD_WRITES
#define WISP_DIAGNOSTIC_ENABLE_SD_TRACE() hypnos.getSDManager()->setWriteDebug(true)
#else
#define WISP_DIAGNOSTIC_ENABLE_SD_TRACE() do {} while (false)
#endif
// END LOOM_BETA_DIAGNOSTICS

// Side-test A0 input: raw ADC and millivolts, plus the existing A7 battery reading.
Loom_Analog analog(manager, A0);

// Source example mux address filter; sensor types and ports are discovered from hardware.
// Keep this example's original sensor support separate from the live bench wiring.
Loom_Multiplexer mux(manager, {0x74, 0x15, 0x6B, 0x44});

// FULL DEBUG ONLY: staged sleep/RTC bench workload, separate from capture flags.
// Disabling diagnostics/trace does not remove this workload. The minimal sibling
// keeps ordinary five-minute sleep instead; compare matching workloads for timing.
// Restart this bench schedule after a reset. Advance only after a checked RTC wake.
// Five wakes at each short interval, then two hours indefinitely; counters never wrap.
constexpr int32_t SLEEP_SLEEP_SECONDS[] = {180, 600, 1800, 7200};
constexpr uint8_t SLEEP_WAKES_PER_STAGE = 5;
// Start recording only after a normal RTC sleep/restore, so the boot-only warmup
// cannot give the first record a different phase. Set false for immediate boot sampling.
constexpr bool SLEEP_ALIGN_FIRST_SAMPLE = true;
bool sleepFirstWakeReady = !SLEEP_ALIGN_FIRST_SAMPLE;
uint32_t sleepLastSavedSampleUtc = 0;
uint32_t sleepSampleExpectedGap = 0;
#if LOOM_TRACE
const char *const SLEEP_SLEEP_LABELS[] = {
    "Sleep sleep stage: 3 minutes", "Sleep sleep stage: 10 minutes",
    "Sleep sleep stage: 30 minutes", "Sleep sleep stage: 2 hours indefinitely"
};
#endif
constexpr uint32_t SLEEP_MAX_ARM_MS = 2000;
constexpr const char *SLEEP_SETTINGS_FILE = "loom_sleep_settings.json";
bool sleepSettingsReady = false;
uint8_t sleepStage = 0;
uint8_t sleepStageWakes = 0;
volatile bool sleepRtcWakeObserved = false;
volatile uint32_t sleepWakeActiveMs = 0;

// Use the same SleepInterval JSON layout that Loom_Hypnos::getConfigFromSD reads.
// This dedicated bench file is overwritten at boot, then when each stage changes.
// Partial writes/readback errors never authorize an RTC sleep or stage advancement.
bool writeSleepSettings(uint8_t stage) {
    FUNCTION_START;
    if (stage >= 4 || !hypnos.getSDManager()->hasSDInitialized()) {
        return false;
    }
    const int32_t seconds = SLEEP_SLEEP_SECONDS[stage];
    char settings[112];
    const int length = snprintf(settings, sizeof(settings),
        "{\"SleepInterval\":{\"days\":0,\"hours\":%ld,\"minutes\":%ld,\"seconds\":%ld}}\r\n",
        static_cast<long>(seconds / 3600), static_cast<long>((seconds % 3600) / 60),
        static_cast<long>(seconds % 60));
    if (length <= 0 || static_cast<size_t>(length) >= sizeof(settings)) {
        return false;
    }
    // Open an independent root through Loom, then a write handle relative to that directory.
    // Do not replace SDManager's retained batch/file handle or create a second SdFat instance.
    File directory = hypnos.getSDManager()->openFile("/");
    if (!directory) {
        return false;
    }
    File output;
    if (!output.open(&directory, SLEEP_SETTINGS_FILE, O_RDWR | O_CREAT)) {
        directory.close();
        return false;
    }
    output.clearWriteError();
    const bool wrote = output.seekSet(0) &&
        output.write(settings, static_cast<size_t>(length)) == length &&
        !output.getWriteError() && output.truncate(static_cast<uint32_t>(length)) && output.sync();
    const bool fileClosed = output.close();
    const bool directoryClosed = directory.close();
    if (!wrote || !fileClosed || !directoryClosed) {
        Serial.println(F("[SLEEP] SD settings write/sync/close failed; interval not accepted"));
        return false;
    }
    const int32_t savedSeconds = hypnos.getConfigFromSD(SLEEP_SETTINGS_FILE).totalseconds();
    if (savedSeconds != seconds) {
        Serial.println(F("[SLEEP] Loom settings readback differs; interval not accepted"));
        return false;
    }
    LOGF("[SLEEP] SD settings saved and loaded by Loom: %s -> %ld seconds",
         SLEEP_SETTINGS_FILE, static_cast<long>(savedSeconds));
    return true;
}

// Advance the bench stage only after verified wakes and verified SD settings.
bool prepareSleepSettings() {
    const uint8_t requestedStage = sleepStageWakes == SLEEP_WAKES_PER_STAGE && sleepStage < 3 ?
        static_cast<uint8_t>(sleepStage + 1) : sleepStage;
    if (!sleepSettingsReady || requestedStage != sleepStage) {
        sleepSettingsReady = writeSleepSettings(requestedStage);
        if (!sleepSettingsReady) {
            return false;
        }
        if (requestedStage != sleepStage) {
            sleepStage = requestedStage;
            sleepStageWakes = 0;
            LOGF("[SLEEP] Stage complete; SD now selects %ld seconds",
                 static_cast<long>(SLEEP_SLEEP_SECONDS[sleepStage]));
#if LOOM_TRACE
            LOOM_TRACE_MARKER(SLEEP_SLEEP_LABELS[sleepStage]);
#endif
        }
    }
    return true;
}

void printSleepRtcTime(const char *label, const DateTime &utc) {
    char timestamp[21];
    hypnos.dateTime_toString(utc, timestamp);
    LOGF("[SLEEP] %s %s", label, timestamp);
}

void isrTrigger() {
    sleepWakeActiveMs = millis(); // Active time only: used to separate preparation/restoration.
    sleepRtcWakeObserved = true;
    hypnos.wakeup();
}

void setup() {

    // TEXT gives live progress/failure context; optional SD copy preserves Logger messages.
    Logger::getInstance()->setDebugOutput(LOOM_DEBUG_TEXT != 0);
#if LOOM_DEBUG_SD_LOG
    ENABLE_SD_LOGGING;
#endif
    hypnos.setWakeWatchdogTimeout(ACTIVE_WATCHDOG_MS);

    // Start the serial interface
    manager.beginSerial();
    // TRACE files complement the text above: Loom starts capture after initialization.
    LOOM_TRACE_ATTACH(manager, hypnos);
#if !LOOM_TRACE
    Serial.println(F("[TRACE] SD trace capture OFF; set LOOM_TRACE to 1 in this sketch to enable"));
#endif
    Serial.println(F("[SLEEP] Source example mux addresses: 0x74, 0x15, 0x6B, 0x44; ports discovered from hardware"));
    Serial.println(F("[SLEEP] Sensor types follow connected mux hardware; analog A0 + battery enabled"));
    Serial.print(F("[SLEEP] Wake cadence stages (seconds): "));
    for (uint8_t stage = 0; stage < 4; ++stage) {
        Serial.print(SLEEP_SLEEP_SECONDS[stage]);
        Serial.print(stage == 3 ? F(" forever; reset starts over\n") : F(" x5, "));
    }
    WISP_DIAGNOSTIC_ENABLE_SD_TRACE();              // LOOM_BETA_DIAGNOSTIC
    WISP_DIAGNOSTIC_ENABLE_MUX_SCAN();              // LOOM_BETA_DIAGNOSTIC
    // Before initialize(): only Serial memory evidence is available; trace starts afterward.
    WISP_DIAGNOSTIC_CHECKPOINT("After global object construction"); // LOOM_BETA_DIAGNOSTIC

    // Set the LTE board to only powerup when a batch is ready to be sent
    lte.setBatchSD(batchSD);

    // Both power rails should be on when awake
    hypnos.setWakeConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

    // Keep both rails on during this bench soak test.
    hypnos.setSleepConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);
    // These bench gas boards stay powered through every standby. Reuse their acquisition
    // settings on normal wakes instead of repeatedly issuing the mode-change command.
    mux.setDFGasPowerRetained(true);

    // Non-interactive fallback if the RTC backup supply was lost in the field.
    hypnos.setCompileTime(__DATE__, __TIME__);

    // Enable the hypnos rails
    hypnos.enable();

    // Synchronize time using LTE.
    hypnos.setNetworkInterface(&lte);

    // Read the MQTT creds file to supply the device with MQTT credentials
    WISP_DIAGNOSTIC_CHECKPOINT("Before loading MQTT settings"); // LOOM_BETA_DIAGNOSTIC
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
    WISP_DIAGNOSTIC_CHECKPOINT("After loading MQTT settings"); // LOOM_BETA_DIAGNOSTIC

    // Initialize the manager (LTE initialization takes ~15 seconds, so do this BEFORE starting the
    // Watchdog)
    WISP_DIAGNOSTIC_CHECKPOINT("Before initializing modules"); // LOOM_BETA_DIAGNOSTIC
    manager.initialize();
    // This boot starts the short-interval test, using a dedicated SD settings file.
    (void)prepareSleepSettings();
// BEGIN LOOM_TRACE_DIAGNOSTICS
#if LOOM_TRACE
    // Object labels below are optional inspector metadata, not needed to start trace.
    // sizeof(object) is its fixed footprint, not all memory owned by that object.
    // Observe objects after automatic startup; earlier allocation times remain unknown.
    if (auto *trace = Loom_Trace::current()) {
        trace->object("Device manager", &manager, sizeof(manager));
        trace->object("Hypnos board and power", &hypnos, sizeof(hypnos), nullptr, -1, hypnos.module_address, hypnos.moduleInitialized);
        trace->object("LTE modem", &lte, sizeof(lte), nullptr, -1, lte.module_address, lte.moduleInitialized);
        trace->object("MQTT publisher", &mqtt, sizeof(mqtt), nullptr, -1, mqtt.module_address, mqtt.moduleInitialized);
        trace->object("SD batch controller", &batchSD, sizeof(batchSD));
        trace->object("Mux sensor controller", &mux, sizeof(mux), nullptr, -1, mux.module_address, mux.moduleInitialized);
        trace->object("A0 analog and battery input", &analog, sizeof(analog), nullptr, -1, analog.module_address, analog.moduleInitialized);
        trace->object("SD file manager", hypnos.getSDManager(), sizeof(SDManager), nullptr, -1, -1, hypnos.getSDManager()->hasSDInitialized());
        trace->object("Sensor JSON document", &manager.getDocument(), sizeof(manager.getDocument()));
        mux.traceObjects();
        LOOM_TRACE_MARKER("Source example mux discovery: supported addresses 0x74, 0x15, 0x6B, 0x44; no fixed gas port mapping");
        LOOM_TRACE_MARKER("Source example mux discovery: all ports available; sensor types from hardware; analog A0");
    }
#endif
// END LOOM_TRACE_DIAGNOSTICS

    WISP_DIAGNOSTIC_CHECKPOINT("After initializing modules"); // LOOM_BETA_DIAGNOSTIC

    // Optional sketch scope: its outermost recorded return is saved automatically.
    FUNCTION_START; // LOOM_TRACE_DIAGNOSTIC

    // Register the ISR and attach to the interrupt
    hypnos.registerInterrupt(isrTrigger);

    WISP_DIAGNOSTIC_CHECKPOINT("Before initial network time sync"); // LOOM_BETA_DIAGNOSTIC
    hypnos.networkTimeUpdate();
    WISP_DIAGNOSTIC_CHECKPOINT("After initial network time sync"); // LOOM_BETA_DIAGNOSTIC

    WISP_DIAGNOSTIC_CHECKPOINT("Setup complete"); // LOOM_BETA_DIAGNOSTIC
}

// Share the exact alarm/standby/restoration checks between startup and ordinary cycles.
// The startup wait does not consume one of the five sampled cycles in a sleep stage.
bool waitForScheduledWake(bool countStageWake);

// Compare persisted packet UTC times; adds serial/trace values, not sensor fields.
void reportSavedSampleCadence() {
    if (hypnos.getSDManager()->getLastLogResult().csv != SDWriteStatus::Saved) {
        return; // Compare only actual persisted records, not a failed write attempt.
    }
    const char *text = manager.getDocument()["timestamp"]["time_utc"].as<const char *>();
    uint32_t actualUtc;
    if (!loomTime::packetUtcSeconds(text, actualUtc)) {
        WARNING(F("[SAMPLE TIMING] Saved record has no valid UTC timestamp; interval cannot be checked"));
        sleepLastSavedSampleUtc = 0;
        return;
    }
    LOOM_TRACE_VALUE("Saved sample UTC (actual packet timestamp)", actualUtc, "UTC epoch seconds");
    if (sleepLastSavedSampleUtc == 0) {
        LOGF("[SAMPLE TIMING] First saved sample timestamp: %s; next saved sample will be compared to this", text);
    } else if (actualUtc < sleepLastSavedSampleUtc) {
        WARNINGF("[SAMPLE TIMING] RTC moved backwards by %lu seconds between saved samples; starting a new comparison",
                 static_cast<unsigned long>(sleepLastSavedSampleUtc - actualUtc));
        LOOM_TRACE_VALUE("Saved sample timestamp moved backwards", sleepLastSavedSampleUtc - actualUtc, "seconds");
    } else {
        const uint32_t actualGap = actualUtc - sleepLastSavedSampleUtc;
        LOGF("[SAMPLE TIMING] Saved timestamp: %s | time since previous saved sample: %lu seconds | configured interval: %lu seconds",
             text, static_cast<unsigned long>(actualGap), static_cast<unsigned long>(sleepSampleExpectedGap));
        LOOM_TRACE_VALUE("Time between saved sample timestamps", actualGap, "seconds");
        LOOM_TRACE_VALUE("Configured interval for this saved sample", sleepSampleExpectedGap, "seconds");
        LOOM_TRACE_VALUE("Saved sample interval error", static_cast<int64_t>(actualGap) - sleepSampleExpectedGap,
                         "seconds (positive=longer, negative=shorter)");
    }
    sleepLastSavedSampleUtc = actualUtc;
}

void loop() {
    FUNCTION_START; // LOOM_TRACE_DIAGNOSTIC

    enableActiveWatchdog();
    WISP_DIAGNOSTIC_BEGIN_CYCLE();            // LOOM_BETA_DIAGNOSTIC
    if (!prepareSleepSettings()) {
        Serial.println(F("[SLEEP] Settings not verified; staying awake and retrying the pending stage"));
        LOOM_TRACE_CHECKPOINT("Sleep SD settings not verified; RTC sleep skipped");
        return;
    }
    if (!sleepFirstWakeReady) {
        // Finish the bounded PM warmup while millis() still runs, before the first alarm
        // anchors the cadence. Retained rails preserve this settled state through standby.
        Serial.println(F("[SLEEP] Finishing initial sensor settling before starting sample timing"));
        mux.prepareForSampling();
        Watchdog.reset();
        Serial.println(F("[SLEEP] Startup complete; first sensor record waits for a verified scheduled RTC wake"));
        LOOM_TRACE_CHECKPOINT("Startup complete; waiting for first scheduled sample wake");
        sleepFirstWakeReady = waitForScheduledWake(false);
        enableActiveWatchdog();
        if (!sleepFirstWakeReady) {
            Serial.println(F("[SLEEP] Initial scheduled wake not verified; no sensor record yet; retrying"));
            return;
        }
    }
    WISP_DIAGNOSTIC_CHECKPOINT("Measurement cycle begins"); // LOOM_BETA_DIAGNOSTIC

    // Measure the data from the sensors
    WISP_DIAGNOSTIC_CHECKPOINT("Before measuring sensors"); // LOOM_BETA_DIAGNOSTIC
    manager.measure();
    WISP_DIAGNOSTIC_CHECKPOINT("After measuring sensors"); // LOOM_BETA_DIAGNOSTIC
#if LOOM_TRACE // LOOM_TRACE_DIAGNOSTIC
    mux.traceObjects(); // LOOM_TRACE_DIAGNOSTIC
#endif // LOOM_TRACE_DIAGNOSTIC

    // Pet the dog again just in case measure took a few seconds
    Watchdog.reset();

    // Package the data into JSON
    manager.package();
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After packaging sensor JSON"); // LOOM_BETA_DIAGNOSTIC

    // Print the JSON document to the Serial monitor
#if LOOM_DEBUG_PRINT_SAMPLES
    WISP_DIAGNOSTIC_CHECKPOINT("Before displaying sensor JSON"); // LOOM_BETA_DIAGNOSTIC
    manager.display_data();
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After displaying sensor JSON"); // LOOM_BETA_DIAGNOSTIC
#endif

    // Log the data to the SD
    WISP_DIAGNOSTIC_CHECKPOINT("Before saving sample and batch to SD"); // LOOM_BETA_DIAGNOSTIC
    if (!hypnos.logToSD()) {
        SDManager *sd = hypnos.getSDManager();
        const SDLogResult result = sd->getLastLogResult();
        if (result.csv == SDWriteStatus::Saved && result.batch == SDWriteStatus::Failed) {
            Watchdog.reset();
            // Only retry a confirmed rolled-back batch append, never the CSV or an uncertain write.
            if (!sd->retryBatch()) {
                WARNING(F("CSV saved, but the one batch-only retry failed."));
            }
        } else {
            ERROR(F(
                "SD logging incomplete; rejected/uncertain writes are not automatically retried."));
        }
    }
    reportSavedSampleCadence();
#if LOOM_TRACE
    const SDLogResult savedSample = hypnos.getSDManager()->getLastLogResult();
    const char *storageStates[] = {"not attempted", "saved", "failed (rolled back)", "rejected", "uncertain"};
    LOOM_TRACE_VALUE("Normal sensor CSV write status", static_cast<uint8_t>(savedSample.csv),
                         storageStates[static_cast<uint8_t>(savedSample.csv)]);
    LOOM_TRACE_VALUE("MQTT batch SD write status", static_cast<uint8_t>(savedSample.batch),
                         storageStates[static_cast<uint8_t>(savedSample.batch)]);
#endif
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After saving sample and batch to SD"); // LOOM_BETA_DIAGNOSTIC

    // Pass in the batchSD to the mqtt obj to check/ publish a batch of data if ready
    WISP_DIAGNOSTIC_CHECKPOINT("Before MQTT publish window"); // LOOM_BETA_DIAGNOSTIC
    const bool networkWindow = batchSD.shouldPublish();
    LOOM_TRACE_VALUE("MQTT publish window due", networkWindow, "boolean (1=yes)");
    if (networkWindow) {
        Watchdog.disable();
    } else {
        Watchdog.reset();
    }
    mqtt.publish(batchSD);
    if (networkWindow) {
        enableActiveWatchdog();
    } else {
        Watchdog.reset();
    }
    WISP_DIAGNOSTIC_CHECKPOINT("After MQTT publish window"); // LOOM_BETA_DIAGNOSTIC

    // Sync time (network updates can also block for several seconds)
    WISP_DIAGNOSTIC_CHECKPOINT("Before network time sync"); // LOOM_BETA_DIAGNOSTIC
    Watchdog.disable();
    hypnos.networkTimeUpdate();
    enableActiveWatchdog();
    WISP_DIAGNOSTIC_CHECKPOINT("After network time sync"); // LOOM_BETA_DIAGNOSTIC

    (void)waitForScheduledWake(true);
}

// Full bench helper: arm a fixed RTC deadline, sleep, and validate wake evidence.
// The ISR only records wake evidence and calls Hypnos; all reporting happens here.
bool waitForScheduledWake(bool countStageWake) {
    FUNCTION_START;
    // Keep wake deadlines on a fixed grid: restoration, sampling and uploads consume
    // part of the interval rather than extending every cycle. Missed slots are skipped.
    // The cadence comes from Loom's SD loader, never directly from the stage table.
    const int32_t sleepSleepSeconds = hypnos.getConfigFromSD(SLEEP_SETTINGS_FILE).totalseconds();
    if (sleepSleepSeconds != SLEEP_SLEEP_SECONDS[sleepStage]) {
        sleepSettingsReady = false; // Authorized bench overwrite will repair it next cycle.
        LOGF("[SLEEP] SD interval missing/invalid/unexpected: %ld s; RTC sleep skipped",
             static_cast<long>(sleepSleepSeconds));
        LOOM_TRACE_CHECKPOINT("Sleep SD interval rejected; RTC sleep skipped");
        return false;
    }
    Serial.print(F("[SLEEP] RTC wake cadence: "));
    Serial.print(sleepSleepSeconds);
    Serial.print(F(" seconds | checked wakes in this stage: "));
    Serial.print(sleepStageWakes);
    Serial.println(sleepStage == 3 ? F(" | two-hour soak stays here forever") : F(" of 5"));
#if LOOM_TRACE
    LOOM_TRACE_MARKER(SLEEP_SLEEP_LABELS[sleepStage]);
    LOOM_TRACE_VALUE("RTC configured wake cadence", sleepSleepSeconds, "seconds between deadlines");
    LOOM_TRACE_VALUE("Sleep sleep stage", sleepStage + 1, "stage (1..4)");
    LOOM_TRACE_VALUE("Sleep checked wakes in current stage", sleepStageWakes, "wakes");
#endif
    WISP_DIAGNOSTIC_CHECKPOINT("Before scheduling RTC wake"); // LOOM_BETA_DIAGNOSTIC
    WISP_DIAGNOSTIC_CHECKPOINT("Before standby"); // LOOM_BETA_DIAGNOSTIC
    // Finish sketch logging/drains before arming. No network work, checkpoints or explicit
    // SD drain sits between a verified alarm and sleep. Hypnos still performs shutdown.
    LOGF("[SLEEP] RTC wake cadence %ld s; stage %u; checked wakes %u",
         static_cast<long>(sleepSleepSeconds), static_cast<unsigned>(sleepStage + 1),
         static_cast<unsigned>(sleepStageWakes));
    Watchdog.reset();
    const uint32_t sleepArmStartMs = millis();
    DateTime sleepRtcBefore;
    const bool sleepRtcBeforeValid = hypnos.tryGetCurrentTime(sleepRtcBefore);
    sleepRtcWakeObserved = false;
    DateTime sleepRtcDeadline;
    const bool sleepAlarmArmed = sleepRtcBeforeValid &&
        hypnos.setSampleInterval(TimeSpan(sleepSleepSeconds), true) &&
        hypnos.getScheduledWakeTime(sleepRtcDeadline) && hypnos.reattachRTCInterrupt();
    const int32_t sleepRemainingSeconds = sleepAlarmArmed &&
        sleepRtcDeadline.unixtime() > sleepRtcBefore.unixtime() ?
        static_cast<int32_t>(sleepRtcDeadline.unixtime() - sleepRtcBefore.unixtime()) : 0;
    const uint32_t sleepArmMs = millis() - sleepArmStartMs;
    if (!sleepAlarmArmed || sleepRemainingSeconds <= 0 || sleepArmMs > SLEEP_MAX_ARM_MS) {
        LOGF("[SLEEP] RTC alarm verification %s; arm took %lu ms (limit 2000); staying awake, stage unchanged",
             sleepAlarmArmed ? "too slow" : "failed", static_cast<unsigned long>(sleepArmMs));
        LOOM_TRACE_VALUE("RTC alarm armed successfully", sleepAlarmArmed, "boolean (1=yes)");
        LOOM_TRACE_VALUE("RTC alarm arming duration", sleepArmMs, "milliseconds");
        LOOM_TRACE_CHECKPOINT("Sleep RTC arm failed or exceeded two-second bound; stage unchanged");
        return false;
    }
    // The RTC uses UTC seconds and checked Alarm 1 date/hour/minute/second readback.
    // SAMD WDT continues in standby: disable it for every interval, including two hours.
    Watchdog.disable();
    hypnos.sleep(false);
    Watchdog.reset();
    DateTime sleepRtcAfter;
    const bool sleepRtcAfterValid = hypnos.tryGetCurrentTime(sleepRtcAfter);
    const uint32_t sleepRtcReadActiveMs = millis(); // Capture before printing or SD logging.
    int32_t sleepElapsedSeconds = -1;
    if (sleepRtcAfterValid && sleepRtcAfter.unixtime() >= sleepRtcBefore.unixtime()) {
        sleepElapsedSeconds = static_cast<int32_t>(sleepRtcAfter.unixtime() - sleepRtcBefore.unixtime());
        printSleepRtcTime("RTC UTC before scheduling:", sleepRtcBefore);
        printSleepRtcTime("RTC UTC scheduled wake deadline:", sleepRtcDeadline);
        printSleepRtcTime("RTC UTC after restoring modules:", sleepRtcAfter);
        Serial.print(F("[SLEEP] RTC elapsed: "));
        Serial.print(sleepElapsedSeconds);
        Serial.println(F(" seconds (includes shutdown and module restoration, not just standby)"));
    }
    const uint32_t sleepWakeMs = sleepWakeActiveMs;
    const uint32_t sleepBeforeWakeActiveMs = sleepWakeMs - sleepArmStartMs;
    const uint32_t sleepAfterWakeActiveMs = sleepRtcReadActiveMs - sleepWakeMs;
    if (sleepRtcWakeObserved) {
        LOGF("[SLEEP] Arm %lu ms; active preparation to wake %lu ms; active restoration to RTC read %lu ms",
             static_cast<unsigned long>(sleepArmMs), static_cast<unsigned long>(sleepBeforeWakeActiveMs),
             static_cast<unsigned long>(sleepAfterWakeActiveMs));
    }
    // A callback can also be delivered by Hypnos when an alarm expires during preparation.
    // Such an awake overrun is not a successful standby test. millis() pauses in SAMD standby.
    const bool sleepStandbyObserved = sleepRtcWakeObserved &&
        sleepBeforeWakeActiveMs + SLEEP_MAX_ARM_MS < static_cast<uint32_t>(sleepRemainingSeconds) * 1000UL;
    LOGF("[SLEEP] Wake cadence %ld s; remaining wait when armed %ld s; RTC elapsed %ld s (includes restoration); callback %s; standby evidence %s",
         static_cast<long>(sleepSleepSeconds), static_cast<long>(sleepRemainingSeconds),
         static_cast<long>(sleepElapsedSeconds),
         sleepRtcWakeObserved ? "observed" : "missing", sleepStandbyObserved ? "present" : "missing");
    // Remove measured active restoration from RTC elapsed. Otherwise an early/noisy wake
    // followed by slow sensor startup could incorrectly pass a short-interval test.
    // Rounded RTC seconds allow 1 s early; the bounded 2 s arm window allows 3 s late.
    const int32_t sleepWakeElapsedEstimate = sleepRtcWakeObserved && sleepElapsedSeconds >= 0 ?
        sleepElapsedSeconds - static_cast<int32_t>(sleepAfterWakeActiveMs / 1000UL) : -1;
    const bool sleepWakeOnTime = sleepRtcWakeObserved && sleepWakeElapsedEstimate >= sleepRemainingSeconds - 1 &&
        sleepWakeElapsedEstimate <= sleepRemainingSeconds + 3;
    LOGF("[SLEEP] Estimated RTC elapsed at wake %ld s; acceptable range %ld..%ld s; %s",
         static_cast<long>(sleepWakeElapsedEstimate), static_cast<long>(sleepRemainingSeconds - 1),
         static_cast<long>(sleepRemainingSeconds + 3), sleepWakeOnTime ? "on time" : "outside range");
    // Captured UTC values are reported after waking, avoiding extra trace I/O while armed.
    LOOM_TRACE_VALUE("RTC UTC captured before scheduling (reported after wake)", sleepRtcBefore.unixtime(), "UTC epoch seconds");
    LOOM_TRACE_VALUE("RTC UTC scheduled wake deadline", sleepRtcDeadline.unixtime(), "UTC epoch seconds");
    LOOM_TRACE_VALUE("RTC remaining wait when armed", sleepRemainingSeconds, "seconds");
    LOOM_TRACE_VALUE("RTC UTC after restoration valid", sleepRtcAfterValid, "boolean (1=yes)");
    if (sleepRtcAfterValid) {
        LOOM_TRACE_VALUE("RTC UTC captured after module restoration", sleepRtcAfter.unixtime(), "UTC epoch seconds");
    }
    LOOM_TRACE_VALUE("RTC alarm arming duration", sleepArmMs, "milliseconds");
    LOOM_TRACE_VALUE("RTC elapsed including module restoration", sleepElapsedSeconds, "seconds (-1=invalid)");
    LOOM_TRACE_VALUE("RTC elapsed at wake estimate", sleepWakeElapsedEstimate, "seconds (-1=invalid)");
    if (sleepRtcWakeObserved && sleepElapsedSeconds >= 0) {
        LOOM_TRACE_VALUE("RTC wake lateness estimate", sleepWakeElapsedEstimate - sleepRemainingSeconds,
                         "seconds (positive=late, negative=early)");
    }
    LOOM_TRACE_VALUE("Sleep RTC wake callback observed", sleepRtcWakeObserved, "boolean (1=yes)");
    if (sleepRtcWakeObserved) {
        LOOM_TRACE_VALUE("Sleep active preparation before wake", sleepBeforeWakeActiveMs, "milliseconds");
        LOOM_TRACE_VALUE("Sleep active restoration after wake", sleepAfterWakeActiveMs, "milliseconds");
    }
    LOOM_TRACE_VALUE("Sleep standby evidence confirmed", sleepStandbyObserved, "boolean (1=yes)");
    LOOM_TRACE_VALUE("RTC wake within timing tolerance", sleepWakeOnTime, "boolean (1=yes)");
    if (sleepStandbyObserved && sleepWakeOnTime) {
        sleepSampleExpectedGap = static_cast<uint32_t>(sleepSleepSeconds);
        if (!countStageWake) {
            Serial.println(F("[SLEEP] First scheduled wake verified; starting the first sensor record; stage count remains zero"));
            LOOM_TRACE_CHECKPOINT("First scheduled wake verified; first sensor record starts next");
        } else {
            if (sleepStageWakes < SLEEP_WAKES_PER_STAGE) {
                ++sleepStageWakes;
            }
            Serial.print(F("[SLEEP] Checked RTC wake returned | stage wakes: "));
            Serial.println(sleepStageWakes);
            LOOM_TRACE_CHECKPOINT("Sleep RTC wake returned on fixed cadence");
            if (!prepareSleepSettings()) {
                Serial.println(F("[SLEEP] Next interval could not be saved/verified; retry before another sleep"));
                LOOM_TRACE_CHECKPOINT("Sleep stage change awaits verified SD settings");
            }
        }
    } else {
        Serial.println(F("[SLEEP] Standby/wake/RTC elapsed not confirmed; repeating this interval, stage unchanged"));
        LOOM_TRACE_CHECKPOINT("Sleep wake not confirmed; stage unchanged");
    }
    WISP_DIAGNOSTIC_CHECKPOINT("After waking and restoring modules"); // LOOM_BETA_DIAGNOSTIC
#if LOOM_TRACE // LOOM_TRACE_DIAGNOSTIC
    mux.traceObjects(); // LOOM_TRACE_DIAGNOSTIC
#endif // LOOM_TRACE_DIAGNOSTIC
    return sleepStandbyObserved && sleepWakeOnTime;
}
