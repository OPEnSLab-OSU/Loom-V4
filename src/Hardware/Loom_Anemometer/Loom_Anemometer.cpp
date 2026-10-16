#include "Loom_Anemometer.h"
 
namespace {
 
// If measure() is called with less than this much counting time behind it (typically right
// after waking from sleep), it waits for the remainder. This only blocks in the wake-up
// case; when the module has been counting continuously, the window is already longer.
// Tradeoff: each pulse is worth 1.492 mph at a 1 s window, so a longer minimum gives
// smoother readings at low wind speeds but keeps the board awake longer.
const uint32_t MIN_SAMPLE_WINDOW_MILLIS = 1000UL;
 
// Ignore pulses closer together than this. Reed switches bounce when they close, which
// would otherwise count one revolution several times. 10 ms allows up to 100 Hz (~149 mph).
const uint32_t MIN_PULSE_INTERVAL_MICROS = 10000UL;
 
// Calibration for the common Argent Data / SparkFun-style anemometer:
// one pulse per second = 1.492 mph. A different sensor model needs a different value.
const float MPH_PER_HZ = 1.492f;
const float KMH_PER_MPH = 1.609344f;
 
const float MILLIS_PER_SECOND = 1000.0f;
}
 
Loom_Anemometer* Loom_Anemometer::activeInstance = nullptr;
volatile uint32_t Loom_Anemometer::pulseCount = 0;
volatile uint32_t Loom_Anemometer::lastPulseMicros = 0;
 
Loom_Anemometer::Loom_Anemometer(Manager& man, uint8_t pin)
    : Module("Loom_Anemometer"), manInst(&man), speedPin(pin) {
    manInst->registerModule(this);
}
 
void Loom_Anemometer::initialize() {
    if (activeInstance != nullptr && activeInstance != this) {
        moduleInitialized = false;
        printModuleName("ERROR: Only one Loom_Anemometer can be active on a board.");
        return;
    }
 
    interruptNumber = digitalPinToInterrupt(speedPin);
    if (interruptNumber == NOT_AN_INTERRUPT) {
        moduleInitialized = false;
        printModuleName("ERROR: Anemometer pin does not support interrupts.");
        return;
    }
 
    // Plain INPUT: assumes the circuit has its own pull-up/pull-down. Use INPUT_PULLUP
    // or INPUT_PULLDOWN here if the switch is wired directly to the pin.
    pinMode(speedPin, INPUT);
 
    activeInstance = this;
    moduleInitialized = true;
 
    // Start counting immediately so a board that never sleeps works without power_up().
    startSampling();
}
 
void Loom_Anemometer::power_up() {
    if (!moduleInitialized) {
        return;
    }
    // Pulses from before sleep are meaningless now, so always begin a fresh window.
    startSampling();
}
 
void Loom_Anemometer::power_down() {
    if (!moduleInitialized) {
        return;
    }
    // Detaching the interrupt is what lets the board stay asleep in wind.
    stopSampling();
}
 
void Loom_Anemometer::startSampling() {
    // The interrupt may already be attached (continuous mode), so the reset must be atomic.
    noInterrupts();
    pulseCount = 0;
    lastPulseMicros = 0;
    interrupts();
 
    sampleStartMillis = millis();
 
    if (!counting) {
        attachInterrupt(interruptNumber, countPulse, RISING);
        counting = true;
    }
}
 
void Loom_Anemometer::stopSampling() {
    if (counting) {
        detachInterrupt(interruptNumber);
        counting = false;
    }
}
 
void Loom_Anemometer::countPulse() {
    const uint32_t now = micros();
 
    // lastPulseMicros == 0 means "no pulse yet in this window". Unsigned subtraction
    // stays correct across micros() rollover.
    if (lastPulseMicros != 0 && now - lastPulseMicros < MIN_PULSE_INTERVAL_MICROS) {
        return;  // Switch bounce
    }
 
    lastPulseMicros = now;
    ++pulseCount;
}
 
void Loom_Anemometer::measure() {
    if (!moduleInitialized) {
        return;
    }
 
    // If power_down() was called and power_up() wasn't, begin counting now.
    if (!counting) {
        startSampling();
    }
 
    // Make sure the window is long enough to give a usable reading.
    const uint32_t elapsedSoFar = millis() - sampleStartMillis;
    if (elapsedSoFar < MIN_SAMPLE_WINDOW_MILLIS) {
        delay(MIN_SAMPLE_WINDOW_MILLIS - elapsedSoFar);
    }
 
    // Read and zero the count in one atomic step: the interrupt is still attached, so a
    // pulse arriving between a separate read and reset would be lost.
    const uint32_t now = millis();
    uint32_t pulses;
    noInterrupts();
    pulses = pulseCount;
    pulseCount = 0;
    interrupts();
 
    // The next window begins where this one ended. lastPulseMicros is deliberately left
    // alone so debounce carries across the boundary.
    const uint32_t elapsedMillis = now - sampleStartMillis;
    sampleStartMillis = now;
 
    lastPulseCount = pulses;
    lastSampleMillis = elapsedMillis;
    windSpeedHz = static_cast<float>(pulses) * MILLIS_PER_SECOND /
                  static_cast<float>(elapsedMillis);  // elapsedMillis >= minimum window, never 0
}
 
float Loom_Anemometer::getWindSpeedMph() const {
    return windSpeedHz * MPH_PER_HZ;
}
 
float Loom_Anemometer::getWindSpeedKmh() const {
    return getWindSpeedMph() * KMH_PER_MPH;
}
 
void Loom_Anemometer::package() {
    if (!moduleInitialized) {
        return;
    }
 
    JsonObject json = manInst->get_data_object(getModuleName());
    json["wind_speed_kmh"] = getWindSpeedKmh();
    json["wind_speed_mph"] = getWindSpeedMph();
    json["wind_speed_hz"] = windSpeedHz;
    json["wind_pulse_count"] = lastPulseCount;
    json["wind_sample_ms"] = lastSampleMillis;
}
 
