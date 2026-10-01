#include "Loom_ReedAnemometer.h"
#include "Logger.h"
#include "Loom_Manager.h"

Loom_ReedAnemometer *volatile Loom_ReedAnemometer::activeSensor = nullptr;

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ReedAnemometer::Loom_ReedAnemometer(Manager &manager, uint8_t pin, float metresPerSecondPerHz,
                                         uint32_t windowMs)
    : Module("ReedAnemometer"), manager(&manager), pin(pin), calibration(metresPerSecondPerHz),
      windowMs(windowMs) {
    manager.registerModule(this);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ReedAnemometer::~Loom_ReedAnemometer() { power_down(); }
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ReedAnemometer::initialize() {
    FUNCTION_START;
    moduleInitialized = false;
    windSpeed = NAN;
    // Validate before digitalPinToInterrupt indexes the board's pin-description array.
    if (pin >= PINS_COUNT || !isfinite(calibration) || calibration <= 0 || windowMs < 100 ||
        windowMs > 5000) {
        ERROR(F("Reed anemometer needs a valid pin/calibration and a 100-5000 ms window."));
        return;
    }
    interruptNumber = digitalPinToInterrupt(pin);
    if (interruptNumber == NOT_AN_INTERRUPT) {
        ERROR(F("Reed anemometer pin has no external interrupt."));
        return;
    }
    pinMode(pin, INPUT_PULLUP);
    moduleInitialized = true;
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ReedAnemometer::countPulse() {
    Loom_ReedAnemometer *sensor = activeSensor;
    if (!sensor) {
        return;
    }
    if (sensor->pulseCount == UINT32_MAX) {
        sensor->overflowed = true;
    } else {
        ++sensor->pulseCount;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ReedAnemometer::measure() {
    FUNCTION_START;
    windSpeed = NAN;
    lastPulseCount = 0;
    if (!moduleInitialized || activeSensor) {
        return;
    }
    pulseCount = 0;
    overflowed = false;
    activeSensor = this;
    const uint32_t start = millis();
    attachInterrupt(interruptNumber, countPulse, FALLING);
    uint32_t lastFeed = start;
    while (static_cast<uint32_t>(millis() - start) < windowMs) {
        delay(1);
        if (static_cast<uint32_t>(millis() - lastFeed) >= 500) {
            LOOM_FEED_WATCHDOG(); // Progress through a bounded sampling window, not an I/O retry.
            lastFeed = millis();
        }
    }
    detachInterrupt(interruptNumber);
    activeSensor = nullptr;
    const uint32_t elapsed = static_cast<uint32_t>(millis() - start);
    lastPulseCount = pulseCount; // The ISR is detached before reading the accumulated count.
    if (!overflowed && elapsed > 0) {
        windSpeed = (static_cast<float>(lastPulseCount) * 1000.0f / elapsed) * calibration;
        if (!isfinite(windSpeed)) {
            windSpeed = NAN;
        }
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ReedAnemometer::package() {
    FUNCTION_START;
    JsonObject json = manager->get_data_object(getModuleName());
    if (isfinite(windSpeed)) {
        json["WindSpeed_mps"] = windSpeed;
    } else {
        json["WindSpeed_mps"] = nullptr;
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ReedAnemometer::power_down() {
    if (activeSensor == this) {
        detachInterrupt(interruptNumber);
        activeSensor = nullptr;
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////
