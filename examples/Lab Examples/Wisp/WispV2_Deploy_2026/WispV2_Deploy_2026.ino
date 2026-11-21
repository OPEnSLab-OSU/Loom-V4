#include <Loom_Manager.h>
#include <Hardware/Loom_BatchSD/Loom_BatchSD.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#define LOOM_MUX_COMPILED_ADDRESSES 0x74, 0x15, 0x6B, 0x44
#include <Hardware/Loom_Multiplexer/Loom_Multiplexer.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Logger.h>
#include <Utilities/Loom_Watchdog.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>

constexpr int ACTIVE_WATCHDOG_MS = 16000;

Manager manager("Deploy_Test_", 8);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

Loom_LTE lte(manager, "hologram", "", "");
Loom_MongoDB mqtt(manager, lte);
// Twelve five-minute records per hour produce one 72-record publish every six hours.
Loom_BatchSD batchSD(hypnos, 72);

// Reads the battery voltage
Loom_Analog analog(manager);

Loom_Multiplexer mux(manager, {LOOM_MUX_COMPILED_ADDRESSES});

void isrTrigger() { hypnos.wakeup(); }

void setup() {

    // Keep warnings/errors available; omit routine debug logs.
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
    mqtt.setClientID(manager.get_serial_num()); // Unique broker client for this Feather.

    // Initialize the manager (LTE initialization takes ~15 seconds, so do this BEFORE starting the
    // Watchdog)
    manager.initialize();

    // Register the ISR and attach to the interrupt
    hypnos.registerInterrupt(isrTrigger);

    hypnos.networkTimeUpdate();
    Watchdog.enable(ACTIVE_WATCHDOG_MS);
}

void loop() {
    Watchdog.reset();
    manager.measure();
    Watchdog.reset();
    manager.package();
    Watchdog.reset();

    if (!hypnos.logToSD()) {
        SDManager *sd = hypnos.getSDManager();
        if (sd == nullptr || !sd->retryBatch()) {
            WARNING(F("SD logging incomplete; check the CSV and upload batch."));
        }
    }
    Watchdog.reset();

    if (batchSD.shouldPublish()) {
        LoomWatchdogPause watchdogPause;
        if (!mqtt.moduleInitialized) {
            mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
        }
        if (mqtt.moduleInitialized) {
            mqtt.publish(batchSD);
        }
        hypnos.networkTimeUpdate();
    }

    // Apply any clock correction before arming the next RTC deadline.
    if (hypnos.scheduleWake(TimeSpan(0, 0, 5, 0))) {
        hypnos.sleep();
    } else {
        LoomWatchdogPause watchdogPause;
        manager.pause(60000);
        hypnos.enable();
    }
}
