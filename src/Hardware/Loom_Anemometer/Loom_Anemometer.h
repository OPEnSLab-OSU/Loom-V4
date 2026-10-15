#pragma once
 
#include <stdint.h>
 
#include "Loom_Manager.h"
#include "Module.h"
 
/**
 * Reports wind speed from a cup anemometer (one pulse per revolution).
 *
 * Pulses are counted by interrupt for the whole time between measure() calls, and the
 * speed is calculated from that count and the real elapsed time. measure() does not block
 * unless the counting window is shorter than the minimum (see MIN_SAMPLE_WINDOW_MILLIS).
 *
 * Sleep behaviour: the pin interrupt is attached only while the module is "counting".
 * power_down() detaches it, so a spinning anemometer cannot wake the board. power_up()
 * re-attaches it and starts a fresh window. Pulses that occur during sleep are NOT counted.
 *
 * Only one Loom_Anemometer can be active on a board at a time.
 */
class Loom_Anemometer : public Module {
public:
    Loom_Anemometer(Manager& man, uint8_t speedPin);
 
    void initialize() override;
    void measure() override;
    void package() override;
    void power_up() override;
    void power_down() override;
 
    float getWindSpeedHz() const { return windSpeedHz; }
    float getWindSpeedMph() const;
    float getWindSpeedKmh() const;
    uint32_t getPulseCount() const { return lastPulseCount; }    // Pulses in the last sample
    uint32_t getSampleMillis() const { return lastSampleMillis; }  // Length of the last sample
 
private:
    static void countPulse();  // ISR: must be static to be usable with attachInterrupt()
 
    void startSampling();  // Zero the count, restart the clock, attach the interrupt
    void stopSampling();   // Detach the interrupt
 
    // State shared with the ISR. Static because the ISR has no "this", which is also why
    // only one instance can exist. volatile because the ISR modifies it asynchronously.
    static volatile uint32_t pulseCount;
    static volatile uint32_t lastPulseMicros;  // For debounce
    static Loom_Anemometer* activeInstance;    // Only used to enforce one instance
 
    Manager* manInst;
    uint8_t speedPin;
    int interruptNumber = -1;  // Set in initialize()
    bool counting = false;     // True while the interrupt is attached
    uint32_t sampleStartMillis = 0;
 
    // Results of the last measure()
    uint32_t lastPulseCount = 0;
    uint32_t lastSampleMillis = 0;
    float windSpeedHz = 0.0f;  // Everything else is derived from this
};
 
