/**
 * Optional Feather M0 health checkpoints, independent of SD. Press 's' in Serial Monitor.
 * Writes are DISABLED until the supply/BMS/BOD and flash-write behavior have been verified.
 * Default spacing is a day, even across resets. Do not write flash after every Manager call.
 * A checkpoint is the last saved health sample, not a claim about the instant of a crash.
 */
#include <Loom_Manager.h>
#include <Logger.h>
#include <Diagnostics/Loom_HealthJournal.h>
#include <Utilities/Loom_MemoryUtils.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <FlashStorage.h>

#ifndef ALLOW_FLASH_HEALTH_WRITES
#define ALLOW_FLASH_HEALTH_WRITES 0
#endif
#ifndef AUTO_FLASH_HEALTH_CHECKPOINTS
#define AUTO_FLASH_HEALTH_CHECKPOINTS 0 // Opt in separately after validating the supply policy.
#endif
#if !defined(__SAMD21G18A__)
#error "This flash-health example targets the SAMD21 Feather M0; review other targets separately."
#endif

using HealthRecord = loomHealth::Record;
// FlashStorage reserves separate aligned erase rows for these objects (256 bytes each on D21).
FlashStorage(healthSlot0, HealthRecord);
FlashStorage(healthSlot1, HealthRecord);
class FlashHealthStorage : public loomHealth::Storage {
  public:
    bool read(uint8_t slot, HealthRecord &record) override {
        if (slot > 1) {
            return false;
        }
        if (slot == 0) {
            healthSlot0.read(&record);
        } else {
            healthSlot1.read(&record);
        }
        return true;
    }
    bool write(uint8_t slot, const HealthRecord &record) override {
        if (slot > 1) {
            return false;
        }
        if (slot == 0) {
            healthSlot0.write(record);
        } else {
            healthSlot1.write(record);
        }
        return true; // The journal verifies the bytes; this SDK does not return write status.
    }
};
FlashHealthStorage storage;
loomHealth::Journal health(storage);
Manager manager("FlashHealthDemo", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false, false); // No SD required.
Loom_Analog analog(manager);

////////////////////////////////////////////////////////////////////////////////////////////////////
const char *configuredRailName(uint32_t value) {
    switch (value) {
    case PR_3V_ON_5V_ON:
        return "3.3V on, 5V on";
    case PR_3V_ON_5V_OFF:
        return "3.3V on, 5V off";
    case PR_3V_OFF_5V_ON:
        return "3.3V off, 5V on";
    case PR_3V_OFF_5V_OFF:
        return "3.3V off, 5V off";
    default:
        return "unavailable";
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void showPreviousHealth() {
    HealthRecord previous;
    if (!health.load(previous)) {
        Serial.println(F("No valid flash checkpoint (new, unreadable or damaged storage)."));
        return;
    }
    Serial.print(F("Last SAVED checkpoint UTC: "));
    Serial.println(previous.utc);
    Serial.print(F("Checkpoint sequence: "));
    Serial.println(previous.sequence);
    Serial.print(F("Phase at checkpoint: "));
    Serial.println(loomHealth::phaseName(previous.health.phase));
    Serial.print(F("Uptime at checkpoint, ms: "));
    Serial.println(previous.health.uptimeMs);
    Serial.print(F("Battery mV (UINT32_MAX means unavailable): "));
    Serial.println(previous.health.batteryMv);
    Serial.print(F("Stack-to-heap gap bytes (UINT32_MAX means unavailable): "));
    Serial.println(previous.health.freeRamBytes);
    Serial.print(F("Sensor mask (bit 0 Analog, bit 1 Hypnos): "));
    Serial.println(previous.health.sensorMask);
    Serial.print(F("Configured wake rails: "));
    Serial.println(configuredRailName(previous.health.railConfig));
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void saveHealthCheckpoint(loomHealth::Phase phase, bool automatic = false) {
    DateTime utc;
    if (!hypnos.tryGetCurrentTime(utc)) {
        Serial.println(F("Checkpoint skipped: RTC did not provide valid UTC."));
        return;
    }
    loomHealth::Snapshot snapshot;
    snapshot.phase = phase; // Most recently completed phase, not the instant of a crash.
    snapshot.uptimeMs = millis();
    const float volts = Loom_Analog::getBatteryVoltage();
    if (isfinite(volts) && volts > 0 && volts <= 20) {
        snapshot.batteryMv = static_cast<uint32_t>(volts * 1000.0f + 0.5f);
    }
    const int freeBytes = LoomMemory::freeMemoryBytes();
    if (freeBytes >= 0) {
        snapshot.freeRamBytes = static_cast<uint32_t>(freeBytes);
    }
    snapshot.sensorMask =
        (analog.moduleInitialized ? 1U : 0U) | (hypnos.moduleInitialized ? 2U : 0U);
    snapshot.railConfig = static_cast<uint32_t>(hypnos.getWakeConfiguration());
    const auto result = health.save(snapshot, utc.unixtime(), ALLOW_FLASH_HEALTH_WRITES != 0);
    // Automatic calls can be frequent; the journal permits at most one write per day.
    if (!automatic || result != loomHealth::SaveResult::TooSoon) {
        Serial.println(loomHealth::saveResultName(result));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void observeManagerHealth(Manager &, Manager::HealthEvent event, void *) {
    if (event == Manager::HealthEvent::BeforeInitialize) {
        showPreviousHealth(); // Print the old checkpoint before this boot initializes sensors.
        return;
    }
    if (!AUTO_FLASH_HEALTH_CHECKPOINTS || !ALLOW_FLASH_HEALTH_WRITES) {
        return; // Default: no automatic ADC, RTC, flash access or additional output.
    }
    if (!loomWatchdogIsEnabled()) {
        return; // Never start an automatic flash write after an operation disabled its guard.
    }
    loomHealth::Phase phase = loomHealth::Phase::Awake;
    switch (event) {
    case Manager::HealthEvent::Initialized:
        phase = loomHealth::Phase::Boot;
        break;
    case Manager::HealthEvent::Measured:
        phase = loomHealth::Phase::Measuring;
        break;
    case Manager::HealthEvent::Packaged:
        phase = loomHealth::Phase::Packaging;
        break;
    case Manager::HealthEvent::PoweredDown:
        phase = loomHealth::Phase::Sleep;
        break;
    default:
        break;
    }
    saveHealthCheckpoint(phase, true);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void setup() {
    manager.beginSerial(false);
    manager.setHealthObserver(observeManagerHealth);
    hypnos.setCompileTime(__DATE__, __TIME__);
    hypnos.enable();
    manager.initialize();  // Prints the actual hardware reset cause separately from the checkpoint.
    Watchdog.enable(8000); // Keep a guard active through the flash SDK; never pause it for a write.
    Serial.println(F("Press 's' for an explicit checkpoint. Flash writes default to disabled."));
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void loop() {
    // Exercise completion events only when this demonstration's automatic option is enabled.
    // Other sketches keep their normal measurement cycle; the observer never drives it.
    static uint32_t previousSampleMs = 0;
    const uint32_t nowMs = millis();
    if (AUTO_FLASH_HEALTH_CHECKPOINTS && static_cast<uint32_t>(nowMs - previousSampleMs) >= 5000) {
        previousSampleMs = nowMs;
        manager.measure();
        manager.package();
    }
    if (Serial.available() && Serial.read() == 's') {
        saveHealthCheckpoint(loomHealth::Phase::Awake);
    }
    LOOM_FEED_WATCHDOG();
    delay(10);
}
////////////////////////////////////////////////////////////////////////////////////////////////////
