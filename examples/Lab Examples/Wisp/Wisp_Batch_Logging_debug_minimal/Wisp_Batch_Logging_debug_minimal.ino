// Minimal trace/heap baseline: ordinary deployment loop plus capture plumbing.
// Compare against Wisp_Batch_Logging_debug for phase checkpoints and bench helpers.
// Compare trace 0/0, 1/0, then 1/1; keep the text controls and wiring/cadence fixed.
// Capture starts AFTER module initialization, so boot allocations are not observed.
// Trace time is active time; standby is excluded. SD capture adds time and RAM overhead.
// See DEBUG-GUIDE.md in the parent Wisp/debug folder for build instructions.

// START HERE: choose the evidence you want, then rebuild.
// TRACE FILES = when calls ran + how RAM changed; inspect after the run.
// DEBUG TEXT = what the device is doing + why a step failed; read while running.
// Both can be ON together. Trace does not require DEBUG text or its SD text copy.
// Trace: /debug/trace_N.perfetto.json + trace_N.ndjson. Text: output_N.log if enabled.
// WARNING/ERROR remain visible with DEBUG text OFF. See DEBUG-GUIDE.md for recipes.

// BEGIN MINIMAL_TRACE_CONTROLS
// 0/0 = no recorder; 1/0 = calls and RAM totals; 1/1 = plus allocator events.
// LOOM_TRACE_HEAP has no effect when LOOM_TRACE is 0.
#ifndef LOOM_TRACE
#define LOOM_TRACE 1
#endif
#ifndef LOOM_TRACE_HEAP
#define LOOM_TRACE_HEAP 1
#endif
// TEXT: progress and failure explanations in Serial Monitor.
// 0 hides routine DEBUG messages/JSON; WARNING and ERROR still print.
#ifndef LOOM_DEBUG_TEXT
#define LOOM_DEBUG_TEXT 0
#endif
// Copy Logger messages to /debug/output_N.log (including warnings/errors).
// Direct Serial memory/mux/SD reports are not copied by this switch.
#ifndef LOOM_DEBUG_SD_LOG
#define LOOM_DEBUG_SD_LOG 0
#endif
// END MINIMAL_TRACE_CONTROLS

// Wisp direct-sensor batch logging example.
#include <Loom_Manager.h>
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

// The optional Loom_TraceHeap companion supplies IDE heap linker hooks on SAMD.
#include <Diagnostics/Loom_TraceSketch.h>

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

void isrTrigger() { hypnos.wakeup(); }

void setup() {
    // Apply the TEXT controls above; TRACE attachment below selects the file evidence.
    Logger::getInstance()->setDebugOutput(LOOM_DEBUG_TEXT != 0);
#if LOOM_DEBUG_SD_LOG
    ENABLE_SD_LOGGING;
#endif
    hypnos.setWakeWatchdogTimeout(ACTIVE_WATCHDOG_MS);

    // Start Serial without waiting for a monitor.
    manager.beginSerial(false);

    // Opt in before initialize(): Loom owns the recorder and handles start/save.
    // With LOOM_TRACE=0 this call does nothing and links no automatic recorder.
    LOOM_TRACE_ATTACH(manager, hypnos);

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
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));

    // Initialize all in-use modules
    manager.initialize();

    // Register the ISR and attach to the interrupt
    hypnos.registerInterrupt(isrTrigger);

    hypnos.networkTimeUpdate();
}

void loop() {

    enableActiveWatchdog();

    // Measure and package the data
    Watchdog.reset();
    manager.measure();
    Watchdog.reset();
    manager.package();
    Watchdog.reset();

    // Log the data to the SD
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

    // Pass in the batchSD to the mqtt obj to check/ publish a batch of data if ready
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

    // Set the interrupt duration for 5 minutes
    Watchdog.reset();
    hypnos.setInterruptDuration(TimeSpan(0, 0, 5, 0));
    Watchdog.reset();

    // Reattach the interrupt
    hypnos.reattachRTCInterrupt();
    Watchdog.reset();

    // Set the hypnos to sleep, but with power still being supplied to the 5v rail (wait for serial
    // when testing from a computer) The SAMD21 watchdog continues in standby, so disable it for the
    // five-minute RTC sleep.
    Watchdog.disable();
    // Hypnos saves pending trace records before taking SD offline.
    hypnos.sleep(false);

    Watchdog.disable(); // Network time may exceed the active watchdog period.
    hypnos.networkTimeUpdate();
    enableActiveWatchdog();
}
