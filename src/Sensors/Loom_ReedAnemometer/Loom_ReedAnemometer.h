#pragma once

#include "Module.h"
#include <math.h>

class Manager;

/**
 * Wind-speed reader for a dry-contact reed switch, such as SparkFun SEN-15901.
 * Counts falling edges during a two-second window; the interrupt is detached afterwards.
 * This is not the driver for a powered analog-output anemometer.
 *
 * Supply the sensor's calibration explicitly in metres/second per pulse/second.
 * SparkFun specifies 1.492 mph per Hz: 1.492 * 0.44704 = 0.666984 m/s per Hz.
 * Wire the contact between an unused interrupt-capable Feather pin and ground.
 * The input uses the Feather's 3.3 V pull-up. Never connect a higher-voltage output.
 * Only one of these modules may own an active measurement window at a time.
 */
class Loom_ReedAnemometer : public Module {
  public:
    Loom_ReedAnemometer(Manager &manager, uint8_t pin, float metresPerSecondPerHz,
                        uint32_t windowMs = 2000);
    ~Loom_ReedAnemometer();
    Loom_ReedAnemometer(const Loom_ReedAnemometer &) = delete;
    Loom_ReedAnemometer &operator=(const Loom_ReedAnemometer &) = delete;

    void initialize() override;
    void measure() override;
    void package() override;
    void power_up() override { initialize(); }
    void power_down() override;
    float getWindSpeed() const { return windSpeed; } // m/s; NaN before/after a failed measurement.
    uint32_t getPulseCount() const { return lastPulseCount; }

  private:
    static void countPulse(); // ISR: count only; no logging, allocation, SD or network work.
    static Loom_ReedAnemometer *volatile activeSensor;
    Manager *manager;
    uint8_t pin;
    int interruptNumber = -1;
    float calibration;
    uint32_t windowMs;
    volatile uint32_t pulseCount = 0;
    volatile bool overflowed = false;
    uint32_t lastPulseCount = 0;
    float windSpeed = NAN;
};
