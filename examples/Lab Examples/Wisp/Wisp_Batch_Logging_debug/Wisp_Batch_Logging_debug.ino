// Full debug sketch: phase-by-phase memory checks and optional trace/heap capture.
// Start with the matching _debug_minimal sketch for a simple flag-only baseline.
// See DEBUG-GUIDE.md for what each layer adds and how to compare builds.

// Wisp direct-sensor batch logging example.
#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Loom_BatchSD/Loom_BatchSD.h>

// BEGIN LOOM_BETA_DIAGNOSTICS
// Default for the extra memory/mux/SD diagnostics below; does NOT disable trace.
// Use the sibling sketch without _debug for deployment. Individual flags override
// this default so one source of diagnostic overhead can be added at a time.
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
// Verbose SD write decisions/results; independent of the structured trace files.
#ifndef LOOM_DEBUG_SD_WRITES
#define LOOM_DEBUG_SD_WRITES LOOM_DEBUG_DIAGNOSTICS
#endif
// Pretty-print sensor JSON; independent of memory checkpoints and trace flags.
#ifndef LOOM_DEBUG_PRINT_SAMPLES
#define LOOM_DEBUG_PRINT_SAMPLES 1
#endif

#if LOOM_DEBUG_MEMORY
#include <Diagnostics/Loom_MemoryDiagnostics.h>
#endif
// END LOOM_BETA_DIAGNOSTICS

#include <Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h>
#include <Sensors/I2C/Loom_SEN55/Loom_SEN55.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Sensors/I2C/Loom_T6793/Loom_T6793.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>

#include <Adafruit_SleepyDog.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Logger.h>

// BEGIN LOOM_TRACE_DIAGNOSTICS
// Structured SD trace: LOOM_TRACE=0 removes recorder; 1 enables calls/RAM.
// LOOM_TRACE_HEAP=1 additionally records bounded allocator windows (requires trace).
// Neither flag enables the serial Loom_MemoryDiagnostics reports above.
// IDE heap capture needs Loom_TraceHeap installed; the CLI launcher supplies hooks.
#ifndef LOOM_TRACE
#define LOOM_TRACE 0
#endif
#ifndef LOOM_TRACE_HEAP
#define LOOM_TRACE_HEAP 0
#endif
#include <Diagnostics/Loom_TraceSketch.h>
LOOM_TRACE_RECORDER(executionTrace);
// END LOOM_TRACE_DIAGNOSTICS

constexpr int ACTIVE_WATCHDOG_MS = 16000;

void enableActiveWatchdog() {
    Watchdog.enable(ACTIVE_WATCHDOG_MS);
    Watchdog.reset();
}

Manager manager("Wisp_brd_v0p4_", 1); // Set a unique deployment identifier for each stack.

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

Loom_Analog analog(manager);

// Main air-quality, temperature, humidity, and CO2 sensors.
Loom_SEN55 SEN55(manager);
Loom_SHT31 sht(manager);

Loom_T6793 T6793(manager);
Loom_DFMultiGasSensor gasSensor(manager, 0x74);

// Connectivity.
Loom_LTE lte(manager, "hologram", "", "");
Loom_MongoDB mqtt(manager, lte);
// Twelve five-minute records per hour produce one 72-record publish every six hours.
Loom_BatchSD batchSD(hypnos, 72);

// BEGIN LOOM_BETA_DIAGNOSTICS
// WISP_DIAGNOSTIC_* prints detailed serial memory reports at named phases.
// LOOM_TRACE_CHECKPOINT beside it records a RAM snapshot in the trace instead.
// They are independent: disabling MEMORY does not remove trace checkpoints.
#if LOOM_DEBUG_MEMORY
Loom_MemoryDiagnostics memoryDiagnostics;
#define WISP_DIAGNOSTIC_BEGIN_CYCLE() memoryDiagnostics.beginCycle()
#define WISP_DIAGNOSTIC_CHECKPOINT(phaseLabel) \
    memoryDiagnostics.checkpoint(F(phaseLabel), manager.getDocument(), batchSD.getCurrentBatch())
#else
#define WISP_DIAGNOSTIC_BEGIN_CYCLE() do {} while (false)
#define WISP_DIAGNOSTIC_CHECKPOINT(phaseLabel) do {} while (false)
#endif
#if LOOM_DEBUG_SD_WRITES
#define WISP_DIAGNOSTIC_ENABLE_SD_TRACE() hypnos.getSDManager()->setWriteDebug(true)
#else
#define WISP_DIAGNOSTIC_ENABLE_SD_TRACE() do {} while (false)
#endif
// END LOOM_BETA_DIAGNOSTICS

void isrTrigger() { hypnos.wakeup(); }

void setup() {
    // Preserve the canonical timestamped debug log in /debug/output_N.log.
    ENABLE_SD_LOGGING;
    hypnos.setWakeWatchdogTimeout(ACTIVE_WATCHDOG_MS);

    // Wait 20 seconds for the serial console to open
    manager.beginSerial();
    WISP_DIAGNOSTIC_ENABLE_SD_TRACE();              // LOOM_BETA_DIAGNOSTIC
    WISP_DIAGNOSTIC_CHECKPOINT("After global object construction"); // LOOM_BETA_DIAGNOSTIC
    // Before TRACE_BEGIN, trace checkpoints are no-ops; serial memory checks still run.
    LOOM_TRACE_CHECKPOINT("After global object construction"); // LOOM_TRACE_DIAGNOSTIC

    // Set the LTE board to only powerup when a batch is ready to be sent
    lte.setBatchSD(batchSD);

    // Both power rails should be on when awake
    hypnos.setWakeConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

    // Only the 5V rail should be on during sleep
    hypnos.setSleepConfiguration(POWERRAIL_CONFIG::PR_3V_OFF_5V_ON);

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

    // Initialize all in-use modules
    WISP_DIAGNOSTIC_CHECKPOINT("Before initializing modules"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before initializing modules"); // LOOM_TRACE_DIAGNOSTIC
    manager.initialize();
// BEGIN LOOM_TRACE_DIAGNOSTICS
#if LOOM_TRACE
    // Object labels below are optional inspector metadata, not needed to start trace.
    // sizeof(object) is its fixed footprint, not all memory owned by that object.
    // Start after SD initialization. Existing objects are observed here; their allocation
    // times remain unknown. New mux sensors and their deletion are tracked thereafter.
    if (LOOM_TRACE_BEGIN(executionTrace, *hypnos.getSDManager())) {
        executionTrace.object("Device manager", &manager, sizeof(manager));
        executionTrace.object("Hypnos board and power", &hypnos, sizeof(hypnos), nullptr, -1, hypnos.module_address, hypnos.moduleInitialized);
        executionTrace.object("LTE modem", &lte, sizeof(lte), nullptr, -1, lte.module_address, lte.moduleInitialized);
        executionTrace.object("MQTT publisher", &mqtt, sizeof(mqtt), nullptr, -1, mqtt.module_address, mqtt.moduleInitialized);
        executionTrace.object("SD batch controller", &batchSD, sizeof(batchSD));
        executionTrace.object("Analog and battery input", &analog, sizeof(analog), nullptr, -1, analog.module_address, analog.moduleInitialized);
        executionTrace.object("Direct SEN55 sensor", &SEN55, sizeof(SEN55), nullptr, -1, SEN55.module_address, SEN55.moduleInitialized);
        executionTrace.object("Direct SHT31 sensor", &sht, sizeof(sht), nullptr, -1, sht.module_address, sht.moduleInitialized);
        executionTrace.object("SD file manager", hypnos.getSDManager(), sizeof(SDManager), nullptr, -1, -1, hypnos.getSDManager()->hasSDInitialized());
        executionTrace.object("Sensor JSON document", &manager.getDocument(), sizeof(manager.getDocument()));
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
    LOOM_TRACE_CHECKPOINT("After initializing modules"); // LOOM_TRACE_DIAGNOSTIC

    // Save guard precedes the function scope so its exit is captured before saving.
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
}

void loop() {
    LOOM_TRACE_SAVE_ON_RETURN();
    FUNCTION_START; // LOOM_TRACE_DIAGNOSTIC

    enableActiveWatchdog();
    WISP_DIAGNOSTIC_BEGIN_CYCLE();            // LOOM_BETA_DIAGNOSTIC
    WISP_DIAGNOSTIC_CHECKPOINT("Measurement cycle begins"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Measurement cycle begins"); // LOOM_TRACE_DIAGNOSTIC

    // Measure and package the data
    WISP_DIAGNOSTIC_CHECKPOINT("Before measuring sensors"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before measuring sensors"); // LOOM_TRACE_DIAGNOSTIC
    Watchdog.reset();
    manager.measure();
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After measuring sensors"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After measuring sensors"); // LOOM_TRACE_DIAGNOSTIC
    manager.package();
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After packaging sensor JSON"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After packaging sensor JSON"); // LOOM_TRACE_DIAGNOSTIC

    // Print the current JSON packet
#if LOOM_DEBUG_PRINT_SAMPLES
    WISP_DIAGNOSTIC_CHECKPOINT("Before displaying sensor JSON"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before displaying sensor JSON"); // LOOM_TRACE_DIAGNOSTIC
    manager.display_data();
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After displaying sensor JSON"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After displaying sensor JSON"); // LOOM_TRACE_DIAGNOSTIC
#endif

    // Log the data to the SD
    WISP_DIAGNOSTIC_CHECKPOINT("Before saving sample and batch to SD"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before saving sample and batch to SD"); // LOOM_TRACE_DIAGNOSTIC
    Watchdog.reset();
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
    LOOM_TRACE_CHECKPOINT("After saving sample and batch to SD"); // LOOM_TRACE_DIAGNOSTIC

    // Pass in the batchSD to the mqtt obj to check/ publish a batch of data if ready
    WISP_DIAGNOSTIC_CHECKPOINT("Before MQTT publish window"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before MQTT publish window"); // LOOM_TRACE_DIAGNOSTIC
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
    LOOM_TRACE_CHECKPOINT("After MQTT publish window"); // LOOM_TRACE_DIAGNOSTIC

    // Set the interrupt duration for 5 minutes
    WISP_DIAGNOSTIC_CHECKPOINT("Before scheduling RTC wake"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before scheduling RTC wake"); // LOOM_TRACE_DIAGNOSTIC
    Watchdog.reset();
    hypnos.setInterruptDuration(TimeSpan(0, 0, 5, 0));
    Watchdog.reset();

    // Reattach the interrupt
    hypnos.reattachRTCInterrupt();
    Watchdog.reset();
    WISP_DIAGNOSTIC_CHECKPOINT("After scheduling RTC wake"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After scheduling RTC wake"); // LOOM_TRACE_DIAGNOSTIC

    // Set the hypnos to sleep, but with power still being supplied to the 5v rail (wait for serial
    // when testing from a computer)
    WISP_DIAGNOSTIC_CHECKPOINT("Before standby"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before standby"); // LOOM_TRACE_DIAGNOSTIC
    // The SAMD21 watchdog continues in standby, so disable it for the five-minute RTC sleep.
    Watchdog.disable();
    LOOM_TRACE_FLUSH(); // LOOM_TRACE_DIAGNOSTIC
    hypnos.sleep(false);
    LOOM_TRACE_FLUSH(); // LOOM_TRACE_DIAGNOSTIC
    WISP_DIAGNOSTIC_CHECKPOINT("After waking and restoring modules"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After waking and restoring modules"); // LOOM_TRACE_DIAGNOSTIC

    WISP_DIAGNOSTIC_CHECKPOINT("Before network time sync"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("Before network time sync"); // LOOM_TRACE_DIAGNOSTIC
    Watchdog.disable(); // Network time may exceed the active watchdog period.
    hypnos.networkTimeUpdate();
    enableActiveWatchdog();
    WISP_DIAGNOSTIC_CHECKPOINT("After network time sync"); // LOOM_BETA_DIAGNOSTIC
    LOOM_TRACE_CHECKPOINT("After network time sync"); // LOOM_TRACE_DIAGNOSTIC
}
