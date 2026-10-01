#define LOOM_SENSOR_FAKE_SAMD 1
#include <cassert>
#include "../../src/Sensors/I2C/Loom_SEN66/Loom_SEN66.cpp"

FakeWire Wire;
FakeWatchdogRegisters registers;
FakeWatchdogRegisters *WDT = &registers;
FakeWatchdog Watchdog;
uint32_t activeMs = 0;
uint32_t pausedDelayMs = 0;
uint32_t millis() { return activeMs; }
void delay(uint32_t ms) {
    if (!registers.CTRL.bit.ENABLE) { pausedDelayMs += ms; }
    activeMs += ms;
}
class Sensor : public Loom_SEN66 {
  public:
    explicit Sensor(Manager &manager) : Loom_SEN66(manager) {}
    using Loom_SEN66::initialize;
    using Loom_SEN66::prepareForSampling;
    using Loom_SEN66::power_up;
    using Loom_SEN66::measure;
};
int main() {
    Manager manager;
    Sensor sensor(manager);
    sensor.initialize(); // An already-running sensor has unknown age: settle conservatively.
    registers.CTRL.bit.ENABLE = 1;
    registers.CONFIG.bit.PER = 11;
    const unsigned int transfers = senFake::transfers;
    const uint32_t before = activeMs;
    const uint32_t beforePaused = pausedDelayMs;
    sensor.prepareForSampling();
    assert(activeMs - before == 30000);
    assert(pausedDelayMs - beforePaused == 30000);
    assert(senFake::transfers == transfers && senFake::readings == 0);
    assert(manager.document.isNull()); // Preparation creates no sample or packet.
    assert(registers.CTRL.bit.ENABLE && registers.CONFIG.bit.PER == 11);
    // Model standby by leaving millis frozen. Retained sensor power preserves settling.
    uint32_t duration = 0;
    const uint32_t afterSettlingPaused = pausedDelayMs;
    for (int wake = 0; wake < 5; ++wake) {
        sensor.power_up();
        const uint32_t start = activeMs;
        sensor.measure();
        assert(activeMs - start == 10500); // First and later windows have the same duration.
        duration = activeMs - start;
        assert(registers.CTRL.bit.ENABLE && registers.CONFIG.bit.PER == 11);
    }
    assert(senFake::resets == 0 && senFake::readings == 50 && duration == 10500);
    assert(pausedDelayMs == afterSettlingPaused); // Every ordinary measurement remains guarded.
    senFake::running = false; // A real sensor power loss must still restart and warm up.
    sensor.power_up();
    assert(senFake::resets == 1);
    const uint32_t coldStart = activeMs;
    sensor.measure();
    assert(activeMs - coldStart == 40500);
    activeMs = UINT32_MAX - 15000;
    Sensor rollover(manager);
    rollover.initialize();
    const uint32_t rolloverStart = activeMs;
    rollover.prepareForSampling();
    assert(activeMs - rolloverStart == 30000); // Settling remains bounded across millis rollover.
}
