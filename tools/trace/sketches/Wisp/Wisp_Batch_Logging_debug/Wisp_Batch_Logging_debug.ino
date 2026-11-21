// Full debug sketch: phase-by-phase memory checks and optional trace/heap capture.
// Start with the matching _debug_minimal sketch for a simple flag-only baseline.
// See DEBUG-GUIDE.md for what each layer adds and how to compare builds.

// Wisp direct-sensor batch logging example.
// START HERE: choose the evidence you want, then rebuild.
// TRACE FILES = when calls ran + how RAM changed; inspect after the run.
// DEBUG TEXT = what the device is doing + why a step failed; read while running.
// Both can be ON together. Trace does not require DEBUG text or its SD text copy.
// Files follow the Manager name: /debug/<name>_trace_N.* and <name>_debug_N.log.
// WARNING/ERROR remain visible with DEBUG text OFF. See DEBUG-GUIDE.md for recipes.
// Optional prefix for all debug files; omit to follow the Manager's device name.
// #define LOOM_DEBUG_LOG_NAME "WispBench"

// TEXT: progress and failure explanations in Serial Monitor.
// 0 hides routine DEBUG messages/JSON; WARNING and ERROR still print.
#ifndef LOOM_DEBUG_TEXT
#define LOOM_DEBUG_TEXT 1
#endif
// Copy Logger messages to /debug/<name>_debug_N.log (including warnings/errors).
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
// Pretty-print sensor JSON through Logger; needs LOOM_DEBUG_TEXT=1 too.
#ifndef LOOM_DEBUG_PRINT_SAMPLES
#define LOOM_DEBUG_PRINT_SAMPLES 1
#endif

// END LOOM_BETA_DIAGNOSTICS

// Discover Loom before its optional diagnostic headers (required by Arduino IDE).
#include <Loom_Manager.h>
#include <Diagnostics/Loom_DebugSketch.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Loom_BatchSD/Loom_BatchSD.h>

#include <Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h>
#include <Sensors/I2C/Loom_SEN55/Loom_SEN55.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Sensors/I2C/Loom_T6793/Loom_T6793.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>

#include <Adafruit_SleepyDog.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Logger.h>

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
// Optional Serial report state. Loom_DebugSketch.h handles the checkpoint flags.
#if LOOM_DEBUG_MEMORY
Loom_MemoryDiagnostics memoryDiagnostics;
#endif
// END LOOM_BETA_DIAGNOSTICS

void isrTrigger() { hypnos.wakeup(); }

void setup() {
#ifdef LOOM_DEBUG_LOG_NAME
    if (!hypnos.getSDManager()->setDebugLogName(LOOM_DEBUG_LOG_NAME)) {
        WARNING(F("Invalid debug log prefix; keep it within 63 characters and set it before SD initialization."));
    }
#endif
    // TEXT gives live progress/failure context; optional SD copy preserves Logger messages.
    Logger::getInstance()->setDebugOutput(LOOM_DEBUG_TEXT != 0);
#if LOOM_DEBUG_SD_LOG
    ENABLE_SD_LOGGING;
#endif
    hypnos.setWakeWatchdogTimeout(ACTIVE_WATCHDOG_MS);

    // Wait 20 seconds for the serial console to open
    manager.beginSerial();
    // TRACE files complement the text above: Loom starts capture after initialization.
    LOOM_TRACE_ATTACH(manager, hypnos);
#if LOOM_DEBUG_SD_WRITES
    hypnos.getSDManager()->setWriteDebug(true); // LOOM_BETA_DIAGNOSTIC
#endif
    // Before initialize(): only Serial memory evidence is available; trace starts afterward.
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After global object construction", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC

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
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Before loading MQTT settings", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After loading MQTT settings", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC

    // Initialize all in-use modules
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Before initializing modules", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
    manager.initialize();
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
        trace->object("Analog and battery input", &analog, sizeof(analog), nullptr, -1, analog.module_address, analog.moduleInitialized);
        trace->object("Direct SEN55 sensor", &SEN55, sizeof(SEN55), nullptr, -1, SEN55.module_address, SEN55.moduleInitialized);
        trace->object("Direct SHT31 sensor", &sht, sizeof(sht), nullptr, -1, sht.module_address, sht.moduleInitialized);
        trace->object("SD file manager", hypnos.getSDManager(), sizeof(SDManager), nullptr, -1, -1, hypnos.getSDManager()->hasSDInitialized());
        trace->object("Sensor JSON document", &manager.getDocument(), sizeof(manager.getDocument()));
    }
#endif
// END LOOM_TRACE_DIAGNOSTICS

    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After initializing modules", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC

    // Optional sketch scope: its outermost recorded return is saved automatically.
    FUNCTION_START; // LOOM_TRACE_DIAGNOSTIC

    // Register the ISR and attach to the interrupt
    hypnos.registerInterrupt(isrTrigger);

    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Before initial network time sync", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
    hypnos.networkTimeUpdate();
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After initial network time sync", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC

    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Setup complete", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
}

void loop() {
    FUNCTION_START; // LOOM_TRACE_DIAGNOSTIC

    enableActiveWatchdog();
    LOOM_DEBUG_BEGIN_CYCLE(memoryDiagnostics);            // LOOM_BETA_DIAGNOSTIC
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Measurement cycle begins", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC

    // Measure and package the data
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Before measuring sensors", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
    Watchdog.reset();
    manager.measure();
    Watchdog.reset();
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After measuring sensors", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
    manager.package();
    Watchdog.reset();
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After packaging sensor JSON", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC

    // Print the current JSON packet
#if LOOM_DEBUG_PRINT_SAMPLES
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Before displaying sensor JSON", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
    manager.display_data();
    Watchdog.reset();
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After displaying sensor JSON", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
#endif

    // Log the data to the SD
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Before saving sample and batch to SD", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
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
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After saving sample and batch to SD", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC

    // Pass in the batchSD to the mqtt obj to check/ publish a batch of data if ready
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Before MQTT publish window", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
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
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After MQTT publish window", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC

    // Set the interrupt duration for 5 minutes
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Before scheduling RTC wake", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
    Watchdog.reset();
    hypnos.setInterruptDuration(TimeSpan(0, 0, 5, 0));
    Watchdog.reset();

    // Reattach the interrupt
    hypnos.reattachRTCInterrupt();
    Watchdog.reset();
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After scheduling RTC wake", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC

    // Set the hypnos to sleep, but with power still being supplied to the 5v rail (wait for serial
    // when testing from a computer)
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Before standby", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
    // The SAMD21 watchdog continues in standby, so disable it for the five-minute RTC sleep.
    Watchdog.disable();
    hypnos.sleep(false);
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After waking and restoring modules", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC

    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "Before network time sync", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
    Watchdog.disable(); // Network time may exceed the active watchdog period.
    hypnos.networkTimeUpdate();
    enableActiveWatchdog();
    LOOM_DEBUG_CHECKPOINT(memoryDiagnostics, "After network time sync", manager.getDocument(), batchSD.getCurrentBatch()); // LOOM_BETA_DIAGNOSTIC
}
