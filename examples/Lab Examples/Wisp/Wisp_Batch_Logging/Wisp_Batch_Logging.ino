// Wisp direct-sensor batch logging example.
#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Loom_BatchSD/Loom_BatchSD.h>

#include <Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h>
#include <Sensors/I2C/Loom_SEN55/Loom_SEN55.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Sensors/I2C/Loom_T6793/Loom_T6793.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>

#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Logger.h>
#include <Utilities/Loom_Watchdog.h>

constexpr int ACTIVE_WATCHDOG_MS = 16000;

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
    // Keep warnings/errors available; omit routine debug logs.
    Logger::getInstance()->setDebugOutput(false);
    hypnos.setWakeWatchdogTimeout(ACTIVE_WATCHDOG_MS);

    manager.beginSerial(false);

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
    mqtt.setClientID(manager.get_serial_num()); // Unique broker client for this Feather.

    // Initialize all in-use modules
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
