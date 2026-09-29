#include <Loom_Manager.h>
#include <Hardware/Loom_BatchSD/Loom_BatchSD.h>

#include <Adafruit_SleepyDog.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Loom_Multiplexer/Loom_Multiplexer.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Logger.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>

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

// Reads the battery voltage
Loom_Analog analog(manager);

Loom_Multiplexer mux(manager, {0x74, 0x15, 0x6B, 0x44});

void isrTrigger() { hypnos.wakeup(); }

void setup() {

    // Keep warnings/errors available; omit routine debug logs and traces.
    Logger::getInstance()->setDebugOutput(false);
    hypnos.setWakeWatchdogTimeout(ACTIVE_WATCHDOG_MS);

    // Start the serial interface
    manager.beginSerial(false);

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
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));

    // Initialize the manager (LTE initialization takes ~15 seconds, so do this BEFORE starting the
    // Watchdog)
    manager.initialize();

    // Register the ISR and attach to the interrupt
    hypnos.registerInterrupt(isrTrigger);

    hypnos.networkTimeUpdate();
}

void loop() {

    enableActiveWatchdog();

    // Measure the data from the sensors
    manager.measure();

    // Pet the dog again just in case measure took a few seconds
    Watchdog.reset();

    // Package the data into JSON
    manager.package();
    Watchdog.reset();

    // Log the data to the SD
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

    // Sync time (network updates can also block for several seconds)
    Watchdog.disable();
    hypnos.networkTimeUpdate();
    enableActiveWatchdog();

    // Set the hypnos to sleep
    // The SAMD21 watchdog continues in standby, so disable it for the five-minute RTC sleep.
    Watchdog.disable();
    hypnos.sleep(false);
}
