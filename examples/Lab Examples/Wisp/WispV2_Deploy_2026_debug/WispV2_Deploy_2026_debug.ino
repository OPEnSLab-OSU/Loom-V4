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

// BEGIN LOOM_TRACE_DIAGNOSTICS
// Extra debug toggle, OFF by default. tools/build_wisp_trace.ps1 sets the matching
// library compiler flag and optional allocator linker hooks for all translation units.
#ifndef LOOM_WISP_TRACE
#define LOOM_WISP_TRACE 0
#endif
#ifndef LOOM_WISP_TRACE_HEAP
#define LOOM_WISP_TRACE_HEAP 0
#endif
#if LOOM_WISP_TRACE
#if !defined(LOOM_ENABLE_TRACE) || !LOOM_ENABLE_TRACE
#error "Use tools/build_wisp_trace.ps1 -Mode calls or -Mode heap for a trace build."
#endif
#include <Diagnostics/Loom_Trace.h>
Loom_Trace executionTrace;
// Declared before the function scope, so its destructor saves the recorded return too.
struct WispTraceSaveOnReturn {
    ~WispTraceSaveOnReturn() {
        if (executionTrace.isRecording() && executionTrace.isStorageAvailable()) {
            executionTrace.flush();
        }
    }
};
#define WISP_TRACE_SCOPE() WispTraceSaveOnReturn traceSaveOnReturn; FUNCTION_START
#define WISP_TRACE_CHECKPOINT(name) executionTrace.memory(name)
#define WISP_TRACE_FLUSH()                                                                         \
    do {                                                                                           \
        if (executionTrace.isRecording() && executionTrace.isStorageAvailable() && !executionTrace.flush()) {                                                             \
            Serial.println(F("[TRACE] capture stopped: SD trace append failed"));                   \
        }                                                                                          \
    } while (false)
#else
#define WISP_TRACE_SCOPE() do {} while (false)
#define WISP_TRACE_CHECKPOINT(name) do {} while (false)
#define WISP_TRACE_FLUSH() do {} while (false)
#endif
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
// Twelve five-minute records per hour produce one 72-record publish every six hours.
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

// Reads the battery voltage
Loom_Analog analog(manager);

Loom_Multiplexer mux(manager, {0x74, 0x15, 0x6B, 0x44});

void isrTrigger() { hypnos.wakeup(); }

void setup() {

    // Preserve the canonical timestamped debug log in /debug/output_N.log.
    ENABLE_SD_LOGGING;
    hypnos.setWakeWatchdogTimeout(ACTIVE_WATCHDOG_MS);

    // Start the serial interface
    manager.beginSerial();
    WISP_DIAGNOSTIC_ENABLE_SD_TRACE();              // LOOM_BETA_DIAGNOSTIC
    WISP_DIAGNOSTIC_ENABLE_MUX_SCAN();              // LOOM_BETA_DIAGNOSTIC
    WISP_DIAGNOSTIC_CHECKPOINT("After global object construction"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("After global object construction"); // LOOM_TRACE_DIAGNOSTIC

    // Set the LTE board to only powerup when a batch is ready to be sent
    lte.setBatchSD(batchSD);

    // Both power rails should be on when awake
    hypnos.setWakeConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

    // Keep both rails on during this bench soak test.
    hypnos.setSleepConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

    // Non-interactive fallback if the RTC backup supply was lost in the field.
    hypnos.setCompileTime(__DATE__, __TIME__);

    // Enable the hypnos rails
    hypnos.enable();

    // Synchronize time using LTE.
    hypnos.setNetworkInterface(&lte);

    // Read the MQTT creds file to supply the device with MQTT credentials
    WISP_DIAGNOSTIC_CHECKPOINT("Before loading MQTT settings"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Before loading MQTT settings"); // LOOM_TRACE_DIAGNOSTIC
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
    WISP_DIAGNOSTIC_CHECKPOINT("After loading MQTT settings"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("After loading MQTT settings"); // LOOM_TRACE_DIAGNOSTIC

    // Initialize the manager (LTE initialization takes ~15 seconds, so do this BEFORE starting the
    // Watchdog)
    WISP_DIAGNOSTIC_CHECKPOINT("Before initializing modules"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Before initializing modules"); // LOOM_TRACE_DIAGNOSTIC
    manager.initialize();
// BEGIN LOOM_TRACE_DIAGNOSTICS
#if LOOM_WISP_TRACE
    // Start after SD initialization. Existing objects are observed here; their allocation
    // times remain unknown. New mux sensors and their deletion are tracked thereafter.
    if (executionTrace.begin(*hypnos.getSDManager(), LOOM_WISP_TRACE_HEAP != 0)) {
        Logger::getInstance()->enableTrace(executionTrace);
        executionTrace.object("Device manager", &manager, sizeof(manager));
        executionTrace.object("Hypnos board and power", &hypnos, sizeof(hypnos), nullptr, -1, hypnos.module_address, hypnos.moduleInitialized);
        executionTrace.object("LTE modem", &lte, sizeof(lte), nullptr, -1, lte.module_address, lte.moduleInitialized);
        executionTrace.object("MQTT publisher", &mqtt, sizeof(mqtt), nullptr, -1, mqtt.module_address, mqtt.moduleInitialized);
        executionTrace.object("SD batch controller", &batchSD, sizeof(batchSD));
        executionTrace.object("Mux sensor controller", &mux, sizeof(mux), nullptr, -1, mux.module_address, mux.moduleInitialized);
        executionTrace.object("Analog and battery input", &analog, sizeof(analog), nullptr, -1, analog.module_address, analog.moduleInitialized);
        executionTrace.object("SD file manager", hypnos.getSDManager(), sizeof(SDManager), nullptr, -1, -1, hypnos.getSDManager()->hasSDInitialized());
        executionTrace.object("Sensor JSON document", &manager.getDocument(), sizeof(manager.getDocument()));
        mux.traceObjects();
        Serial.print(F("[TRACE] Open this SD file directly in Perfetto: "));
        Serial.println(executionTrace.getPerfettoPath());
        Serial.print(F("[TRACE] Load this SD file in the detailed heap/object inspector: "));
        Serial.println(executionTrace.getRecordPath());
        Serial.println(F("[TRACE] Active time excludes standby; baseline allocation times are unknown"));
    } else {
        Serial.println(F("[TRACE] Could not start optional SD capture"));
    }
#endif
// END LOOM_TRACE_DIAGNOSTICS

    WISP_DIAGNOSTIC_CHECKPOINT("After initializing modules"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("After initializing modules"); // LOOM_TRACE_DIAGNOSTIC

    WISP_TRACE_SCOPE(); // LOOM_TRACE_DIAGNOSTIC

    // Register the ISR and attach to the interrupt
    hypnos.registerInterrupt(isrTrigger);

    WISP_DIAGNOSTIC_CHECKPOINT("Before initial network time sync"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Before initial network time sync"); // LOOM_TRACE_DIAGNOSTIC
    hypnos.networkTimeUpdate();
    WISP_DIAGNOSTIC_CHECKPOINT("After initial network time sync"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("After initial network time sync"); // LOOM_TRACE_DIAGNOSTIC

    WISP_DIAGNOSTIC_CHECKPOINT("Setup complete"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Setup complete"); // LOOM_TRACE_DIAGNOSTIC
    WISP_TRACE_FLUSH(); // LOOM_TRACE_DIAGNOSTIC
}

void loop() {

    WISP_TRACE_SCOPE(); // LOOM_TRACE_DIAGNOSTIC

    enableActiveWatchdog();
    WISP_DIAGNOSTIC_BEGIN_CYCLE();            // LOOM_BETA_DIAGNOSTIC
    WISP_DIAGNOSTIC_CHECKPOINT("Measurement cycle begins"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Measurement cycle begins"); // LOOM_TRACE_DIAGNOSTIC

    // Measure the data from the sensors
    WISP_DIAGNOSTIC_CHECKPOINT("Before measuring sensors"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Before measuring sensors"); // LOOM_TRACE_DIAGNOSTIC
    manager.measure();
    WISP_DIAGNOSTIC_CHECKPOINT("After measuring sensors"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("After measuring sensors"); // LOOM_TRACE_DIAGNOSTIC
#if LOOM_WISP_TRACE // LOOM_TRACE_DIAGNOSTIC
    mux.traceObjects(); // LOOM_TRACE_DIAGNOSTIC
#endif // LOOM_TRACE_DIAGNOSTIC

    // Pet the dog again just in case measure took a few seconds
    Watchdog.reset();

    // Package the data into JSON
    manager.package();
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After packaging sensor JSON"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("After packaging sensor JSON"); // LOOM_TRACE_DIAGNOSTIC

    // Print the JSON document to the Serial monitor
    WISP_DIAGNOSTIC_CHECKPOINT("Before displaying sensor JSON"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Before displaying sensor JSON"); // LOOM_TRACE_DIAGNOSTIC
    manager.display_data();
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After displaying sensor JSON"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("After displaying sensor JSON"); // LOOM_TRACE_DIAGNOSTIC

    // Log the data to the SD
    WISP_DIAGNOSTIC_CHECKPOINT("Before saving sample and batch to SD"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Before saving sample and batch to SD"); // LOOM_TRACE_DIAGNOSTIC
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
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After saving sample and batch to SD"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("After saving sample and batch to SD"); // LOOM_TRACE_DIAGNOSTIC

    // Pass in the batchSD to the mqtt obj to check/ publish a batch of data if ready
    WISP_DIAGNOSTIC_CHECKPOINT("Before MQTT publish window"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Before MQTT publish window"); // LOOM_TRACE_DIAGNOSTIC
    const bool networkWindow = batchSD.shouldPublish();
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
    WISP_TRACE_CHECKPOINT("After MQTT publish window"); // LOOM_TRACE_DIAGNOSTIC

    // Set the interrupt duration for 5 minutes
    WISP_DIAGNOSTIC_CHECKPOINT("Before scheduling RTC wake"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Before scheduling RTC wake"); // LOOM_TRACE_DIAGNOSTIC
    Watchdog.reset();
    hypnos.setInterruptDuration(TimeSpan(0, 0, 5, 0));
    Watchdog.reset();

    // Reattach the interrupt
    hypnos.reattachRTCInterrupt();
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After scheduling RTC wake"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("After scheduling RTC wake"); // LOOM_TRACE_DIAGNOSTIC

    // Sync time (network updates can also block for several seconds)
    WISP_DIAGNOSTIC_CHECKPOINT("Before network time sync"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Before network time sync"); // LOOM_TRACE_DIAGNOSTIC
    Watchdog.disable();
    hypnos.networkTimeUpdate();
    enableActiveWatchdog();
    WISP_DIAGNOSTIC_CHECKPOINT("After network time sync"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("After network time sync"); // LOOM_TRACE_DIAGNOSTIC

    // Set the hypnos to sleep
    WISP_DIAGNOSTIC_CHECKPOINT("Before standby"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("Before standby"); // LOOM_TRACE_DIAGNOSTIC
    // The SAMD21 watchdog continues in standby, so disable it for the five-minute RTC sleep.
    Watchdog.disable();
    WISP_TRACE_FLUSH(); // LOOM_TRACE_DIAGNOSTIC
    hypnos.sleep(false);
    WISP_DIAGNOSTIC_CHECKPOINT("After waking and restoring modules"); // LOOM_BETA_DIAGNOSTIC
    WISP_TRACE_CHECKPOINT("After waking and restoring modules"); // LOOM_TRACE_DIAGNOSTIC
#if LOOM_WISP_TRACE // LOOM_TRACE_DIAGNOSTIC
    mux.traceObjects(); // LOOM_TRACE_DIAGNOSTIC
#endif // LOOM_TRACE_DIAGNOSTIC
    WISP_TRACE_FLUSH(); // LOOM_TRACE_DIAGNOSTIC
}
