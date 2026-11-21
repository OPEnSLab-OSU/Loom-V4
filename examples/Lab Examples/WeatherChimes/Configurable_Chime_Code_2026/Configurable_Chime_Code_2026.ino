#include <Loom_Manager.h>
#include <Logger.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Loom_TippingBucket/Loom_TippingBucket.h>
#include <Sensors/Analog/Loom_Teros10/Loom_Teros10.h>
#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Sensors/I2C/Loom_TSL2591/Loom_TSL2591.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>

#define USE_LTE true
#define BATCH_UPLOAD true
constexpr uint8_t TIP_PIN = A0;
constexpr uint8_t BATCH_SIZE = 72;
constexpr uint16_t WATCHDOG_MS = 16000;
constexpr uint8_t TIP_DEBOUNCE_MS = 20;

Manager manager("Device_Name", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);
Loom_Analog analog(manager);
Loom_SHT31 sht(manager);
Loom_TSL2591 tsl(manager);
Loom_MS5803 ms_water(manager, 119);
Loom_MS5803 ms_air(manager, 118);
Loom_TippingBucket bucket(manager, COUNTER_TYPE::MANUAL, 0.01f);
Loom_Teros10 teros(manager, A1);

#if USE_LTE
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
Loom_LTE lte(manager, "hologram", "", "");
Loom_MongoDB mqtt(manager, lte);
#if BATCH_UPLOAD
#include <Hardware/Loom_BatchSD/Loom_BatchSD.h>
Loom_BatchSD batchSD(hypnos, BATCH_SIZE);
#endif
#endif

TimeSpan sleepInterval;
volatile bool sampleDue = true;
volatile uint8_t pendingTips = 0;

void rtcWake() {
    sampleDue = true;
    hypnos.wakeup();
}

void tipWake() {
    static uint32_t lastTip = 0;
    static bool seenTip = false;
    const uint32_t now = millis();
    if (seenTip && now - lastTip < TIP_DEBOUNCE_MS) {
        return;
    }
    lastTip = now;
    seenTip = true;
    hypnos.shouldPowerUp = false;
    if (pendingTips < UINT8_MAX) {
        ++pendingTips;
    }
}

void setup() {
    Logger::getInstance()->setDebugOutput(false);
    manager.beginSerial(false);
    pinMode(TIP_PIN, INPUT);
    hypnos.setWakeWatchdogTimeout(WATCHDOG_MS);
    // This stack keeps the rain-gauge and sensor rails powered in standby.
    hypnos.setWakeConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);
    hypnos.setSleepConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);
    hypnos.setCompileTime(__DATE__, __TIME__);
#if USE_LTE
#if BATCH_UPLOAD
    lte.setBatchSD(batchSD);
#endif
    hypnos.setNetworkInterface(&lte);
#endif
    hypnos.enable();
    sleepInterval = hypnos.getConfigFromSD("SD_config.json");
    bucket.setHypnosInstance(hypnos);
#if USE_LTE
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
    mqtt.setClientID(manager.get_serial_num());
#endif
    manager.initialize();
    hypnos.registerInterrupt(rtcWake);
    attachInterrupt(digitalPinToInterrupt(TIP_PIN), tipWake, FALLING);
}

void loop() {
    noInterrupts();
    uint8_t tips = pendingTips;
    pendingTips = 0;
    interrupts();
    if (tips > 0) {
        delay(TIP_DEBOUNCE_MS); // Keep millis() running through the contact bounce before standby.
    }
    while (tips > 0) {
        bucket.incrementCount();
        --tips;
    }

    if (sampleDue && !hypnos.isSecondaryWake()) {
        Watchdog.enable(WATCHDOG_MS);
        if (!hypnos.setSampleInterval(sleepInterval)) {
            Watchdog.disable();
            manager.pause(60000);
            hypnos.enable();
            return;
        }
        sampleDue = false;

        tsl.wake_up();
        manager.measure();
        tsl.sleep();
        manager.package();
        if (ms_water.moduleInitialized && ms_air.moduleInitialized) {
            const float height = (ms_water.getPressure() - ms_air.getPressure()) * 100 /
                                 (997.77f * 9.81f);
            manager.addData("Water", "Height_(m)", height);
        }

        if (!hypnos.logToSD()) {
            WARNING(F("Chimes sample could not be fully saved to SD."));
        }
#if USE_LTE
#if BATCH_UPLOAD
        const bool uploadDue = batchSD.shouldPublish();
#else
        constexpr bool uploadDue = true;
#endif
        if (uploadDue) {
            if (!mqtt.moduleInitialized) {
                mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
            }
            if (mqtt.moduleInitialized) {
#if BATCH_UPLOAD
                mqtt.publish(batchSD);
#else
                mqtt.publish();
#endif
            }
        }
#endif
    }

    Watchdog.disable();
    hypnos.sleep();
}
