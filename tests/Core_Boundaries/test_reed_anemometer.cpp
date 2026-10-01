#include <assert.h>
#include <math.h>
#include "../../src/Sensors/Loom_ReedAnemometer/Loom_ReedAnemometer.cpp"

uint32_t now = 0;
uint32_t ticks = 0;
void (*interruptCallback)() = nullptr;
bool generatePulses = true;
unsigned attachments = 0, detachments = 0;
uint32_t millis() { return now; }
void delay(uint32_t ms) {
    now += ms;
    ticks += ms;
    if (generatePulses && interruptCallback && ticks % 500 == 0) {
        interruptCallback(); // Two closures per second, no pulses generated outside the window.
    }
}
void pinMode(int, int) {}
int digitalPinToInterrupt(uint8_t pin) { return pin == 31 ? NOT_AN_INTERRUPT : pin; }
void attachInterrupt(int, void (*callback)(), int mode) {
    assert(!interruptCallback && mode == FALLING);
    interruptCallback = callback;
    ++attachments;
}
void detachInterrupt(int) {
    interruptCallback = nullptr;
    ++detachments;
}

int main() {
    Manager manager;
    Loom_ReedAnemometer wind(manager, 5, 0.5f);
    assert(isnan(wind.getWindSpeed()));
    wind.initialize();
    now = UINT32_MAX - 999;
    wind.measure();
    wind.package();
    assert(wind.getPulseCount() == 4 && wind.getWindSpeed() == 1.0f);
    assert(ticks == 2000 && !interruptCallback && attachments == detachments);
    assert(manager.document["ReedAnemometer"]["WindSpeed_mps"].as<float>() == 1.0f);
    delay(5000);
    assert(wind.getPulseCount() == 4); // No off-window accumulation.
    wind.measure();
    assert(wind.getPulseCount() == 4); // A new window replaces, rather than adds to, the old count.
    generatePulses = false;
    wind.measure();
    assert(wind.getPulseCount() == 0 && wind.getWindSpeed() == 0); // Valid calm reading.

    for (uint8_t badPin : {uint8_t(31), uint8_t(32), uint8_t(255)}) {
        Loom_ReedAnemometer invalid(manager, badPin, 0.5f);
        const unsigned before = attachments;
        invalid.initialize();
        invalid.measure();
        invalid.package();
        assert(attachments == before && isnan(invalid.getWindSpeed()));
        assert(manager.document["ReedAnemometer"]["WindSpeed_mps"].isNull());
    }
    for (float calibration : {0.0f, -1.0f, float(NAN), float(INFINITY)}) {
        Loom_ReedAnemometer invalid(manager, 5, calibration);
        invalid.initialize();
        invalid.measure();
        assert(isnan(invalid.getWindSpeed()));
    }
    for (uint32_t window : {uint32_t(0), uint32_t(99), uint32_t(5001), UINT32_MAX}) {
        Loom_ReedAnemometer invalid(manager, 5, 0.5f, window);
        invalid.initialize();
        invalid.measure();
        assert(isnan(invalid.getWindSpeed()));
    }
}
