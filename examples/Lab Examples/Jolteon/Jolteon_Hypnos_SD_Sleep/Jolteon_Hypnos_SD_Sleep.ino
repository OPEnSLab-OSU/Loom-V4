/**
 * Hypnos RTC sleep and SD time logger.
 *
 * Each cycle:
 *   1. Schedules a DS3231 alarm.
 *   2. Packages the current UTC and local RTC timestamps.
 *   3. Writes the packet to SD.
 *   4. Powers down the switched rails and enters standby.
 *   5. Reinitializes the SD card after RTC wake before the next write.
 *
 * Hardware:
 *   Hypnos V3.3
 *   RTC interrupt on GPIO12
 *   SD chip select on GPIO11
 *   SD card powered from 3VRAIL
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

static constexpr uint32_t SLEEP_SECONDS = 30;
static constexpr uint8_t RTC_INTERRUPT_PIN = 12;
static constexpr uint8_t SD_CHIP_SELECT_PIN = 11;
static constexpr uint8_t SD_RECOVERY_ATTEMPTS = 3;
static constexpr uint32_t SD_RECOVERY_DELAY_MS = 500;

Manager manager("HypnosTime", 1);

Loom_Hypnos hypnos(
    manager,
    HYPNOS_VERSION::V3_3,
    TIME_ZONE::PST,
    false,
    true
);

static volatile bool rtcWakeSeen = false;
static uint32_t cycleNumber = 0;
static bool rtcInterruptReady = false;

static void rtcWakeISR() {
    rtcWakeSeen = true;
    hypnos.wakeup();
}

static bool ensureSDReady() {
    SDManager *sdManager = hypnos.getSDManager();

    if (sdManager == nullptr) {
        Serial.println(F("SD recovery failed: SD support is disabled."));
        return false;
    }

    if (sdManager->hasSDInitialized()) {
        return true;
    }

    Serial.println(F("SD card is not initialized. Starting wake recovery."));

    for (uint8_t attempt = 1; attempt <= SD_RECOVERY_ATTEMPTS; ++attempt) {
        pinMode(SD_CHIP_SELECT_PIN, OUTPUT);
        digitalWrite(SD_CHIP_SELECT_PIN, HIGH);

        Serial.print(F("SD recovery attempt "));
        Serial.print(attempt);
        Serial.print(F(" of "));
        Serial.println(SD_RECOVERY_ATTEMPTS);

        delay(SD_RECOVERY_DELAY_MS);

        if (sdManager->begin()) {
            Serial.println(F("SD card recovered."));
            return true;
        }
    }

    Serial.println(F("SD card recovery failed."));
    return false;
}

void setup() {
    manager.beginSerial(true);

    Serial.println();
    Serial.println(F("Hypnos RTC sleep and SD time logger"));
    Serial.println(F("Hypnos version: V3.3"));
    Serial.println(F("RTC wake pin: GPIO12"));
    Serial.println(F("SD chip select: GPIO11"));
    Serial.println(F("Sleep rails: 3.3V OFF, 5V OFF"));

    Serial.print(F("Wake interval: "));
    Serial.print(SLEEP_SECONDS);
    Serial.println(F(" seconds"));

    hypnos.setCompileTime(__DATE__, __TIME__);
    hypnos.setLogName("HypnosTime");

    /*
     * The DS3231 uses the fixed +3V3 supply and remains alive while the
     * switched 3VRAIL and 5VRAIL are disabled. Power-cycling 3VRAIL also
     * gives the SD card a clean shutdown while its CS pin becomes an input.
     */
    hypnos.setSleepConfiguration(PR_3V_OFF_5V_OFF);
    hypnos.setWakeConfiguration(PR_3V_ON_5V_OFF);

    Serial.println(F("Enabling the 3.3V rail, SD card, and RTC."));
    hypnos.enable(true, false);

    manager.initialize();

    rtcInterruptReady = hypnos.registerInterrupt(
        rtcWakeISR,
        RTC_INTERRUPT_PIN,
        SLEEP,
        LOW
    );

    if (!rtcInterruptReady) {
        Serial.println(F("RTC wake interrupt registration failed."));
        Serial.println(F("The logger will remain awake."));
    }
}

void loop() {
    if (rtcWakeSeen) {
        noInterrupts();
        rtcWakeSeen = false;
        interrupts();

        Serial.println(F("RTC wake interrupt received."));

        /*
         * post_sleep() has already restored 3VRAIL and waited one second.
         * Retry SD initialization now, after the card supply has stabilized.
         */
        ensureSDReady();
    }

    ++cycleNumber;

    Serial.println();
    Serial.print(F("Starting log cycle "));
    Serial.println(cycleNumber);

    hypnos.setInterruptDuration(
        TimeSpan(0, 0, 0, SLEEP_SECONDS)
    );

    manager.measure();
    manager.package();

    manager.addData(
        "SleepTest",
        "Cycle",
        cycleNumber
    );

    manager.addData(
        "SleepTest",
        "WakeIntervalSeconds",
        SLEEP_SECONDS
    );

    manager.display_data();

    const bool sdReady = ensureSDReady();
    const bool logged = sdReady && hypnos.logToSD();

    Serial.println(
        logged
            ? F("SD log write succeeded.")
            : F("SD log write failed.")
    );

    if (!rtcInterruptReady) {
        Serial.println(F("Skipping standby because the RTC interrupt is unavailable."));
        manager.pause(5000);
        return;
    }

    Serial.println(F("Entering RTC standby sleep."));
    Serial.flush();

    hypnos.sleep(false);
}