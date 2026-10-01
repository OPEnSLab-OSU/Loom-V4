#include <Loom_Manager.h>
#include <Hardware/Loom_BatchSD/Loom_BatchSD.h>

// BEGIN LOOM_BETA_DIAGNOSTICS
// Soak-test diagnostics. Use the sibling sketch without _debug for deployment.
// Set this switch to 0 to compare builds without memory/mux/SD trace instrumentation.
#ifndef LOOM_WISP_BETA_DIAGNOSTICS
#define LOOM_WISP_BETA_DIAGNOSTICS 1
#endif

#if LOOM_WISP_BETA_DIAGNOSTICS
#include <Diagnostics/Loom_MemoryDiagnostics.h>
#endif
// END LOOM_BETA_DIAGNOSTICS

#include <Adafruit_SleepyDog.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Loom_Multiplexer/Loom_Multiplexer.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Logger.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Utilities/Loom_TimeUtils.h>

// BEGIN LOOM_TRACE_DIAGNOSTICS
// Reusable Loom sketch controls, independent of ordinary debug logging.
// Heap allocation capture additionally needs the heap build's linker hooks.
#ifndef LOOM_TRACE
#define LOOM_TRACE 1
#endif
#ifndef LOOM_TRACE_HEAP
#define LOOM_TRACE_HEAP 1
#endif
#include <Diagnostics/Loom_TraceSketch.h>
LOOM_TRACE_RECORDER(executionTrace);
// END LOOM_TRACE_DIAGNOSTICS

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
#if LOOM_WISP_BETA_DIAGNOSTICS
Loom_MemoryDiagnostics memoryDiagnostics;
#define WISP_DIAGNOSTIC_BEGIN_CYCLE() memoryDiagnostics.beginCycle()
#define WISP_DIAGNOSTIC_CHECKPOINT(phaseLabel)                                                     \
    memoryDiagnostics.checkpoint(F(phaseLabel), manager.getDocument(), batchSD.getCurrentBatch())
#define WISP_DIAGNOSTIC_ENABLE_MUX_SCAN()                                                          \
    do {                                                                                           \
        mux.setDebug(true);                                                                        \
        mux.setScanDebug(true);                                                                    \
    } while (false)
#define WISP_DIAGNOSTIC_ENABLE_SD_TRACE() hypnos.getSDManager()->setWriteDebug(true)
#else
#define WISP_DIAGNOSTIC_BEGIN_CYCLE()                                                              \
    do {                                                                                           \
    } while (false)
#define WISP_DIAGNOSTIC_CHECKPOINT(phaseLabel)                                                     \
    do {                                                                                           \
    } while (false)
#define WISP_DIAGNOSTIC_ENABLE_MUX_SCAN()                                                          \
    do {                                                                                           \
    } while (false)
#define WISP_DIAGNOSTIC_ENABLE_SD_TRACE()                                                          \
    do {                                                                                           \
    } while (false)
#endif
// END LOOM_BETA_DIAGNOSTICS

// Side-test A0 input: raw ADC and millivolts, plus the existing A7 battery reading.
Loom_Analog analog(manager, A0);

// Source example mux address filter; sensor types and ports are discovered from hardware.
// Keep this example's original sensor support separate from the live bench wiring.
Loom_Multiplexer mux(manager, {0x74, 0x15, 0x6B, 0x44});

// Restart this bench schedule after a reset. Advance only after a checked RTC wake.
// Five wakes at each short interval, then two hours indefinitely; counters never wrap.
constexpr int32_t STRESS_SLEEP_SECONDS[] = {180, 600, 1800, 7200};
constexpr uint8_t STRESS_WAKES_PER_STAGE = 5;
// Start recording only after a normal RTC sleep/restore, so the boot-only warmup
// cannot give the first record a different phase. Set false for immediate boot sampling.
constexpr bool STRESS_ALIGN_FIRST_SAMPLE = true;
bool stressFirstWakeReady = !STRESS_ALIGN_FIRST_SAMPLE;
uint32_t stressLastSavedSampleUtc = 0;
uint32_t stressSampleExpectedGap = 0;
#if LOOM_TRACE
const char *const STRESS_SLEEP_LABELS[] = {
    "Stress sleep stage: 3 minutes", "Stress sleep stage: 10 minutes",
    "Stress sleep stage: 30 minutes", "Stress sleep stage: 2 hours indefinitely"
};
#endif
constexpr uint32_t STRESS_MAX_ARM_MS = 2000;
constexpr const char *STRESS_SETTINGS_FILE = "wisp_stress_settings.json";
bool stressSettingsReady = false;
uint8_t stressSleepStage = 0;
uint8_t stressStageWakes = 0;
volatile bool stressRtcWakeObserved = false;
volatile uint32_t stressWakeActiveMs = 0;

// Use the same SleepInterval JSON layout that Loom_Hypnos::getConfigFromSD reads.
// This dedicated bench file is overwritten at boot, then when each stage changes.
// Partial writes/readback errors never authorize an RTC sleep or stage advancement.
bool writeStressSettings(uint8_t stage) {
    FUNCTION_START;
    if (stage >= 4 || !hypnos.getSDManager()->hasSDInitialized()) {
        return false;
    }
    const int32_t seconds = STRESS_SLEEP_SECONDS[stage];
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
    if (!output.open(&directory, STRESS_SETTINGS_FILE, O_RDWR | O_CREAT)) {
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
        Serial.println(F("[STRESS] SD settings write/sync/close failed; interval not accepted"));
        return false;
    }
    const int32_t savedSeconds = hypnos.getConfigFromSD(STRESS_SETTINGS_FILE).totalseconds();
    if (savedSeconds != seconds) {
        Serial.println(F("[STRESS] Loom settings readback differs; interval not accepted"));
        return false;
    }
    LOGF("[STRESS] SD settings saved and loaded by Loom: %s -> %ld seconds",
         STRESS_SETTINGS_FILE, static_cast<long>(savedSeconds));
    return true;
}

bool prepareStressSettings() {
    const uint8_t requestedStage = stressStageWakes == STRESS_WAKES_PER_STAGE && stressSleepStage < 3 ?
        static_cast<uint8_t>(stressSleepStage + 1) : stressSleepStage;
    if (!stressSettingsReady || requestedStage != stressSleepStage) {
        stressSettingsReady = writeStressSettings(requestedStage);
        if (!stressSettingsReady) {
            return false;
        }
        if (requestedStage != stressSleepStage) {
            stressSleepStage = requestedStage;
            stressStageWakes = 0;
            LOGF("[STRESS] Stage complete; SD now selects %ld seconds",
                 static_cast<long>(STRESS_SLEEP_SECONDS[stressSleepStage]));
#if LOOM_TRACE
            executionTrace.marker(STRESS_SLEEP_LABELS[stressSleepStage]);
#endif
        }
    }
    return true;
}

void printStressRtcTime(const char *label, const DateTime &utc) {
    char timestamp[21];
    hypnos.dateTime_toString(utc, timestamp);
    LOGF("[STRESS] %s %s", label, timestamp);
}

void isrTrigger() {
    stressWakeActiveMs = millis(); // Active time only: used to separate preparation/restoration.
    stressRtcWakeObserved = true;
    hypnos.wakeup();
}

void setup() {

    // Preserve the canonical timestamped debug log in /debug/output_N.log.
    ENABLE_SD_LOGGING;
    hypnos.setWakeWatchdogTimeout(ACTIVE_WATCHDOG_MS);

    // Start the serial interface
    manager.beginSerial();
#if !LOOM_TRACE
    Serial.println(F("[TRACE] SD trace capture OFF; set LOOM_TRACE to 1 in this sketch to enable"));
#endif
    Serial.println(F("[STRESS] Source example mux addresses: 0x74, 0x15, 0x6B, 0x44; ports discovered from hardware"));
    Serial.println(F("[STRESS] Sensor types follow connected mux hardware; analog A0 + battery enabled"));
    Serial.print(F("[STRESS] Wake cadence stages (seconds): "));
    for (uint8_t stage = 0; stage < 4; ++stage) {
        Serial.print(STRESS_SLEEP_SECONDS[stage]);
        Serial.print(stage == 3 ? F(" forever; reset starts over\n") : F(" x5, "));
    }
    WISP_DIAGNOSTIC_ENABLE_SD_TRACE();              // LOOM_BETA_DIAGNOSTIC
    WISP_DIAGNOSTIC_ENABLE_MUX_SCAN();              // LOOM_BETA_DIAGNOSTIC
    WISP_DIAGNOSTIC_CHECKPOINT("After global object construction"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After global object construction"); // LOOM_TRACE_DIAGNOSTIC

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
    LOOM_TRACE_CHECKPOINT("Before loading MQTT settings"); // LOOM_TRACE_DIAGNOSTIC
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
    WISP_DIAGNOSTIC_CHECKPOINT("After loading MQTT settings"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After loading MQTT settings"); // LOOM_TRACE_DIAGNOSTIC

    // Initialize the manager (LTE initialization takes ~15 seconds, so do this BEFORE starting the
    // Watchdog)
    WISP_DIAGNOSTIC_CHECKPOINT("Before initializing modules"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before initializing modules"); // LOOM_TRACE_DIAGNOSTIC
    manager.initialize();
    // This boot starts the short-interval test, using a dedicated SD settings file.
    (void)prepareStressSettings();
// BEGIN LOOM_TRACE_DIAGNOSTICS
#if LOOM_TRACE
    // Start after SD initialization. Existing objects are observed here; their allocation
    // times remain unknown. New mux sensors and their deletion are tracked thereafter.
    if (LOOM_TRACE_BEGIN(executionTrace, *hypnos.getSDManager())) {
        executionTrace.object("Device manager", &manager, sizeof(manager));
        executionTrace.object("Hypnos board and power", &hypnos, sizeof(hypnos), nullptr, -1, hypnos.module_address, hypnos.moduleInitialized);
        executionTrace.object("LTE modem", &lte, sizeof(lte), nullptr, -1, lte.module_address, lte.moduleInitialized);
        executionTrace.object("MQTT publisher", &mqtt, sizeof(mqtt), nullptr, -1, mqtt.module_address, mqtt.moduleInitialized);
        executionTrace.object("SD batch controller", &batchSD, sizeof(batchSD));
        executionTrace.object("Mux sensor controller", &mux, sizeof(mux), nullptr, -1, mux.module_address, mux.moduleInitialized);
        executionTrace.object("A0 analog and battery input", &analog, sizeof(analog), nullptr, -1, analog.module_address, analog.moduleInitialized);
        executionTrace.object("SD file manager", hypnos.getSDManager(), sizeof(SDManager), nullptr, -1, -1, hypnos.getSDManager()->hasSDInitialized());
        executionTrace.object("Sensor JSON document", &manager.getDocument(), sizeof(manager.getDocument()));
        mux.traceObjects();
        executionTrace.marker("Source example mux discovery: supported addresses 0x74, 0x15, 0x6B, 0x44; no fixed gas port mapping");
        executionTrace.marker("Source example mux discovery: all ports available; sensor types from hardware; analog A0");
        Serial.print(F("[TRACE] Open this SD file directly in Perfetto: "));
        Serial.println(executionTrace.getPerfettoPath());
        Serial.print(F("[TRACE] Load this SD file in the detailed heap/object inspector: "));
        Serial.println(executionTrace.getRecordPath());
        Serial.println(F("[TRACE] Active time excludes standby; baseline allocation times are unknown"));
        Serial.println(LOOM_TRACE_HEAP ?
            F("[TRACE] Allocation hooks requested; see the trace session metadata for hook availability") :
            F("[TRACE] Calls/objects/free-RAM checkpoints ON; individual allocations OFF (heap build enables them)"));
    } else {
        Serial.println(F("[TRACE] Could not start optional SD capture"));
    }
#endif
// END LOOM_TRACE_DIAGNOSTICS

    WISP_DIAGNOSTIC_CHECKPOINT("After initializing modules"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After initializing modules"); // LOOM_TRACE_DIAGNOSTIC

    LOOM_TRACE_SAVE_ON_RETURN();
    FUNCTION_START; // LOOM_TRACE_DIAGNOSTIC

    // Register the ISR and attach to the interrupt
    hypnos.registerInterrupt(isrTrigger);

    WISP_DIAGNOSTIC_CHECKPOINT("Before initial network time sync"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before initial network time sync"); // LOOM_TRACE_DIAGNOSTIC
    hypnos.networkTimeUpdate();
    WISP_DIAGNOSTIC_CHECKPOINT("After initial network time sync"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After initial network time sync"); // LOOM_TRACE_DIAGNOSTIC

    WISP_DIAGNOSTIC_CHECKPOINT("Setup complete"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Setup complete"); // LOOM_TRACE_DIAGNOSTIC
    LOOM_TRACE_FLUSH(); // LOOM_TRACE_DIAGNOSTIC
}

// Share the exact alarm/standby/restoration checks between startup and ordinary cycles.
// The startup wait does not consume one of the five sampled cycles in a stress stage.
bool waitForStressWake(bool countStageWake);

void reportSavedSampleCadence() {
    if (hypnos.getSDManager()->getLastLogResult().csv != SDWriteStatus::Saved) {
        return; // Compare only actual persisted records, not a failed write attempt.
    }
    const char *text = manager.getDocument()["timestamp"]["time_utc"].as<const char *>();
    uint32_t actualUtc;
    if (!loomTime::packetUtcSeconds(text, actualUtc)) {
        WARNING(F("[SAMPLE TIMING] Saved record has no valid UTC timestamp; interval cannot be checked"));
        stressLastSavedSampleUtc = 0;
        return;
    }
    LOOM_TRACE_VALUE("Saved sample UTC (actual packet timestamp)", actualUtc, "UTC epoch seconds");
    if (stressLastSavedSampleUtc == 0) {
        LOGF("[SAMPLE TIMING] First saved sample timestamp: %s; next saved sample will be compared to this", text);
    } else if (actualUtc < stressLastSavedSampleUtc) {
        WARNINGF("[SAMPLE TIMING] RTC moved backwards by %lu seconds between saved samples; starting a new comparison",
                 static_cast<unsigned long>(stressLastSavedSampleUtc - actualUtc));
        LOOM_TRACE_VALUE("Saved sample timestamp moved backwards", stressLastSavedSampleUtc - actualUtc, "seconds");
    } else {
        const uint32_t actualGap = actualUtc - stressLastSavedSampleUtc;
        LOGF("[SAMPLE TIMING] Saved timestamp: %s | time since previous saved sample: %lu seconds | configured interval: %lu seconds",
             text, static_cast<unsigned long>(actualGap), static_cast<unsigned long>(stressSampleExpectedGap));
        LOOM_TRACE_VALUE("Time between saved sample timestamps", actualGap, "seconds");
        LOOM_TRACE_VALUE("Configured interval for this saved sample", stressSampleExpectedGap, "seconds");
        LOOM_TRACE_VALUE("Saved sample interval error", static_cast<int64_t>(actualGap) - stressSampleExpectedGap,
                         "seconds (positive=longer, negative=shorter)");
    }
    stressLastSavedSampleUtc = actualUtc;
}

void loop() {

    LOOM_TRACE_SAVE_ON_RETURN();
    FUNCTION_START; // LOOM_TRACE_DIAGNOSTIC

    enableActiveWatchdog();
    WISP_DIAGNOSTIC_BEGIN_CYCLE();            // LOOM_BETA_DIAGNOSTIC
    if (!prepareStressSettings()) {
        Serial.println(F("[STRESS] Settings not verified; staying awake and retrying the pending stage"));
        LOOM_TRACE_CHECKPOINT("Stress SD settings not verified; RTC sleep skipped");
        return;
    }
    if (!stressFirstWakeReady) {
        // Finish the bounded PM warmup while millis() still runs, before the first alarm
        // anchors the cadence. Retained rails preserve this settled state through standby.
        Serial.println(F("[STRESS] Finishing initial sensor settling before starting sample timing"));
        mux.prepareForSampling();
        Watchdog.reset();
        Serial.println(F("[STRESS] Startup complete; first sensor record waits for a verified scheduled RTC wake"));
        LOOM_TRACE_CHECKPOINT("Startup complete; waiting for first scheduled sample wake");
        stressFirstWakeReady = waitForStressWake(false);
        enableActiveWatchdog();
        if (!stressFirstWakeReady) {
            Serial.println(F("[STRESS] Initial scheduled wake not verified; no sensor record yet; retrying"));
            return;
        }
    }
    WISP_DIAGNOSTIC_CHECKPOINT("Measurement cycle begins"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Measurement cycle begins"); // LOOM_TRACE_DIAGNOSTIC

    // Measure the data from the sensors
    WISP_DIAGNOSTIC_CHECKPOINT("Before measuring sensors"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before measuring sensors"); // LOOM_TRACE_DIAGNOSTIC
    manager.measure();
    WISP_DIAGNOSTIC_CHECKPOINT("After measuring sensors"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After measuring sensors"); // LOOM_TRACE_DIAGNOSTIC
#if LOOM_TRACE // LOOM_TRACE_DIAGNOSTIC
    mux.traceObjects(); // LOOM_TRACE_DIAGNOSTIC
#endif // LOOM_TRACE_DIAGNOSTIC

    // Pet the dog again just in case measure took a few seconds
    Watchdog.reset();

    // Package the data into JSON
    manager.package();
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After packaging sensor JSON"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After packaging sensor JSON"); // LOOM_TRACE_DIAGNOSTIC

    // Print the JSON document to the Serial monitor
    WISP_DIAGNOSTIC_CHECKPOINT("Before displaying sensor JSON"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before displaying sensor JSON"); // LOOM_TRACE_DIAGNOSTIC
    manager.display_data();
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After displaying sensor JSON"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After displaying sensor JSON"); // LOOM_TRACE_DIAGNOSTIC

    // Log the data to the SD
    WISP_DIAGNOSTIC_CHECKPOINT("Before saving sample and batch to SD"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before saving sample and batch to SD"); // LOOM_TRACE_DIAGNOSTIC
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
    executionTrace.value("Normal sensor CSV write status", static_cast<uint8_t>(savedSample.csv),
                         storageStates[static_cast<uint8_t>(savedSample.csv)]);
    executionTrace.value("MQTT batch SD write status", static_cast<uint8_t>(savedSample.batch),
                         storageStates[static_cast<uint8_t>(savedSample.batch)]);
#endif
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After saving sample and batch to SD"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After saving sample and batch to SD"); // LOOM_TRACE_DIAGNOSTIC

    // Pass in the batchSD to the mqtt obj to check/ publish a batch of data if ready
    WISP_DIAGNOSTIC_CHECKPOINT("Before MQTT publish window"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before MQTT publish window"); // LOOM_TRACE_DIAGNOSTIC
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
    LOOM_TRACE_CHECKPOINT("After MQTT publish window"); // LOOM_TRACE_DIAGNOSTIC

    // Sync time (network updates can also block for several seconds)
    WISP_DIAGNOSTIC_CHECKPOINT("Before network time sync"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before network time sync"); // LOOM_TRACE_DIAGNOSTIC
    Watchdog.disable();
    hypnos.networkTimeUpdate();
    enableActiveWatchdog();
    WISP_DIAGNOSTIC_CHECKPOINT("After network time sync"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After network time sync"); // LOOM_TRACE_DIAGNOSTIC

    (void)waitForStressWake(true);
}

bool waitForStressWake(bool countStageWake) {
    LOOM_TRACE_SAVE_ON_RETURN();
    FUNCTION_START;
    // Keep wake deadlines on a fixed grid: restoration, sampling and uploads consume
    // part of the interval rather than extending every cycle. Missed slots are skipped.
    // The cadence comes from Loom's SD loader, never directly from the stage table.
    const int32_t stressSleepSeconds = hypnos.getConfigFromSD(STRESS_SETTINGS_FILE).totalseconds();
    if (stressSleepSeconds != STRESS_SLEEP_SECONDS[stressSleepStage]) {
        stressSettingsReady = false; // Authorized bench overwrite will repair it next cycle.
        LOGF("[STRESS] SD interval missing/invalid/unexpected: %ld s; RTC sleep skipped",
             static_cast<long>(stressSleepSeconds));
        LOOM_TRACE_CHECKPOINT("Stress SD interval rejected; RTC sleep skipped");
        LOOM_TRACE_FLUSH();
        return false;
    }
    Serial.print(F("[STRESS] RTC wake cadence: "));
    Serial.print(stressSleepSeconds);
    Serial.print(F(" seconds | checked wakes in this stage: "));
    Serial.print(stressStageWakes);
    Serial.println(stressSleepStage == 3 ? F(" | two-hour soak stays here forever") : F(" of 5"));
#if LOOM_TRACE
    executionTrace.marker(STRESS_SLEEP_LABELS[stressSleepStage]);
    executionTrace.value("RTC configured wake cadence", stressSleepSeconds, "seconds between deadlines");
    executionTrace.value("Sleep stress stage", stressSleepStage + 1, "stage (1..4)");
    executionTrace.value("Sleep checked wakes in current stage", stressStageWakes, "wakes");
#endif
    WISP_DIAGNOSTIC_CHECKPOINT("Before scheduling RTC wake"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before scheduling RTC wake"); // LOOM_TRACE_DIAGNOSTIC
    WISP_DIAGNOSTIC_CHECKPOINT("Before standby"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before standby"); // LOOM_TRACE_DIAGNOSTIC
    // Finish sketch logging/drains before arming. No network work, checkpoints or explicit
    // SD drain sits between a verified alarm and sleep. Hypnos still performs shutdown.
    LOGF("[STRESS] RTC wake cadence %ld s; stage %u; checked wakes %u",
         static_cast<long>(stressSleepSeconds), static_cast<unsigned>(stressSleepStage + 1),
         static_cast<unsigned>(stressStageWakes));
    LOOM_TRACE_FLUSH();
    Watchdog.reset();
    const uint32_t stressArmStartMs = millis();
    DateTime stressRtcBefore;
    const bool stressRtcBeforeValid = hypnos.tryGetCurrentTime(stressRtcBefore);
    stressRtcWakeObserved = false;
    DateTime stressRtcDeadline;
    const bool stressAlarmArmed = stressRtcBeforeValid &&
        hypnos.setSampleInterval(TimeSpan(stressSleepSeconds), true) &&
        hypnos.getScheduledWakeTime(stressRtcDeadline) && hypnos.reattachRTCInterrupt();
    const int32_t stressRemainingSeconds = stressAlarmArmed &&
        stressRtcDeadline.unixtime() > stressRtcBefore.unixtime() ?
        static_cast<int32_t>(stressRtcDeadline.unixtime() - stressRtcBefore.unixtime()) : 0;
    const uint32_t stressArmMs = millis() - stressArmStartMs;
    if (!stressAlarmArmed || stressRemainingSeconds <= 0 || stressArmMs > STRESS_MAX_ARM_MS) {
        LOGF("[STRESS] RTC alarm verification %s; arm took %lu ms (limit 2000); staying awake, stage unchanged",
             stressAlarmArmed ? "too slow" : "failed", static_cast<unsigned long>(stressArmMs));
        LOOM_TRACE_VALUE("RTC alarm armed successfully", stressAlarmArmed, "boolean (1=yes)");
        LOOM_TRACE_VALUE("RTC alarm arming duration", stressArmMs, "milliseconds");
        LOOM_TRACE_CHECKPOINT("Stress RTC arm failed or exceeded two-second bound; stage unchanged");
        LOOM_TRACE_FLUSH();
        return false;
    }
    // The RTC uses UTC seconds and checked Alarm 1 date/hour/minute/second readback.
    // SAMD WDT continues in standby: disable it for every interval, including two hours.
    Watchdog.disable();
    hypnos.sleep(false);
    Watchdog.reset();
    DateTime stressRtcAfter;
    const bool stressRtcAfterValid = hypnos.tryGetCurrentTime(stressRtcAfter);
    const uint32_t stressRtcReadActiveMs = millis(); // Capture before printing or SD logging.
    int32_t stressElapsedSeconds = -1;
    if (stressRtcAfterValid && stressRtcAfter.unixtime() >= stressRtcBefore.unixtime()) {
        stressElapsedSeconds = static_cast<int32_t>(stressRtcAfter.unixtime() - stressRtcBefore.unixtime());
        printStressRtcTime("RTC UTC before scheduling:", stressRtcBefore);
        printStressRtcTime("RTC UTC scheduled wake deadline:", stressRtcDeadline);
        printStressRtcTime("RTC UTC after restoring modules:", stressRtcAfter);
        Serial.print(F("[STRESS] RTC elapsed: "));
        Serial.print(stressElapsedSeconds);
        Serial.println(F(" seconds (includes shutdown and module restoration, not just standby)"));
    }
    const uint32_t stressWakeMs = stressWakeActiveMs;
    const uint32_t stressBeforeWakeActiveMs = stressWakeMs - stressArmStartMs;
    const uint32_t stressAfterWakeActiveMs = stressRtcReadActiveMs - stressWakeMs;
    if (stressRtcWakeObserved) {
        LOGF("[STRESS] Arm %lu ms; active preparation to wake %lu ms; active restoration to RTC read %lu ms",
             static_cast<unsigned long>(stressArmMs), static_cast<unsigned long>(stressBeforeWakeActiveMs),
             static_cast<unsigned long>(stressAfterWakeActiveMs));
    }
    // A callback can also be delivered by Hypnos when an alarm expires during preparation.
    // Such an awake overrun is not a successful standby test. millis() pauses in SAMD standby.
    const bool stressStandbyObserved = stressRtcWakeObserved &&
        stressBeforeWakeActiveMs + STRESS_MAX_ARM_MS < static_cast<uint32_t>(stressRemainingSeconds) * 1000UL;
    LOGF("[STRESS] Wake cadence %ld s; remaining wait when armed %ld s; RTC elapsed %ld s (includes restoration); callback %s; standby evidence %s",
         static_cast<long>(stressSleepSeconds), static_cast<long>(stressRemainingSeconds),
         static_cast<long>(stressElapsedSeconds),
         stressRtcWakeObserved ? "observed" : "missing", stressStandbyObserved ? "present" : "missing");
    // Remove measured active restoration from RTC elapsed. Otherwise an early/noisy wake
    // followed by slow sensor startup could incorrectly pass a short-interval test.
    // Rounded RTC seconds allow 1 s early; the bounded 2 s arm window allows 3 s late.
    const int32_t stressWakeElapsedEstimate = stressRtcWakeObserved && stressElapsedSeconds >= 0 ?
        stressElapsedSeconds - static_cast<int32_t>(stressAfterWakeActiveMs / 1000UL) : -1;
    const bool stressWakeOnTime = stressRtcWakeObserved && stressWakeElapsedEstimate >= stressRemainingSeconds - 1 &&
        stressWakeElapsedEstimate <= stressRemainingSeconds + 3;
    LOGF("[STRESS] Estimated RTC elapsed at wake %ld s; acceptable range %ld..%ld s; %s",
         static_cast<long>(stressWakeElapsedEstimate), static_cast<long>(stressRemainingSeconds - 1),
         static_cast<long>(stressRemainingSeconds + 3), stressWakeOnTime ? "on time" : "outside range");
    // Captured UTC values are reported after waking, avoiding extra trace I/O while armed.
    LOOM_TRACE_VALUE("RTC UTC captured before scheduling (reported after wake)", stressRtcBefore.unixtime(), "UTC epoch seconds");
    LOOM_TRACE_VALUE("RTC UTC scheduled wake deadline", stressRtcDeadline.unixtime(), "UTC epoch seconds");
    LOOM_TRACE_VALUE("RTC remaining wait when armed", stressRemainingSeconds, "seconds");
    LOOM_TRACE_VALUE("RTC UTC after restoration valid", stressRtcAfterValid, "boolean (1=yes)");
    if (stressRtcAfterValid) {
        LOOM_TRACE_VALUE("RTC UTC captured after module restoration", stressRtcAfter.unixtime(), "UTC epoch seconds");
    }
    LOOM_TRACE_VALUE("RTC alarm arming duration", stressArmMs, "milliseconds");
    LOOM_TRACE_VALUE("RTC elapsed including module restoration", stressElapsedSeconds, "seconds (-1=invalid)");
    LOOM_TRACE_VALUE("RTC elapsed at wake estimate", stressWakeElapsedEstimate, "seconds (-1=invalid)");
    if (stressRtcWakeObserved && stressElapsedSeconds >= 0) {
        LOOM_TRACE_VALUE("RTC wake lateness estimate", stressWakeElapsedEstimate - stressRemainingSeconds,
                         "seconds (positive=late, negative=early)");
    }
    LOOM_TRACE_VALUE("Sleep RTC wake callback observed", stressRtcWakeObserved, "boolean (1=yes)");
    if (stressRtcWakeObserved) {
        LOOM_TRACE_VALUE("Sleep active preparation before wake", stressBeforeWakeActiveMs, "milliseconds");
        LOOM_TRACE_VALUE("Sleep active restoration after wake", stressAfterWakeActiveMs, "milliseconds");
    }
    LOOM_TRACE_VALUE("Sleep standby evidence confirmed", stressStandbyObserved, "boolean (1=yes)");
    LOOM_TRACE_VALUE("RTC wake within timing tolerance", stressWakeOnTime, "boolean (1=yes)");
    if (stressStandbyObserved && stressWakeOnTime) {
        stressSampleExpectedGap = static_cast<uint32_t>(stressSleepSeconds);
        if (!countStageWake) {
            Serial.println(F("[STRESS] First scheduled wake verified; starting the first sensor record; stage count remains zero"));
            LOOM_TRACE_CHECKPOINT("First scheduled wake verified; first sensor record starts next");
        } else {
            if (stressStageWakes < STRESS_WAKES_PER_STAGE) {
                ++stressStageWakes;
            }
            Serial.print(F("[STRESS] Checked RTC wake returned | stage wakes: "));
            Serial.println(stressStageWakes);
            LOOM_TRACE_CHECKPOINT("Stress RTC wake returned on fixed cadence");
            if (!prepareStressSettings()) {
                Serial.println(F("[STRESS] Next interval could not be saved/verified; retry before another sleep"));
                LOOM_TRACE_CHECKPOINT("Stress stage change awaits verified SD settings");
            }
        }
    } else {
        Serial.println(F("[STRESS] Standby/wake/RTC elapsed not confirmed; repeating this interval, stage unchanged"));
        LOOM_TRACE_CHECKPOINT("Stress wake not confirmed; stage unchanged");
    }
    WISP_DIAGNOSTIC_CHECKPOINT("After waking and restoring modules"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After waking and restoring modules"); // LOOM_TRACE_DIAGNOSTIC
#if LOOM_TRACE // LOOM_TRACE_DIAGNOSTIC
    mux.traceObjects(); // LOOM_TRACE_DIAGNOSTIC
#endif // LOOM_TRACE_DIAGNOSTIC
    LOOM_TRACE_FLUSH(); // LOOM_TRACE_DIAGNOSTIC
    return stressStandbyObserved && stressWakeOnTime;
}
