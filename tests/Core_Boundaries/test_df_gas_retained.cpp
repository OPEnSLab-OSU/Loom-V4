#include <cassert>
#include "../../src/Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.cpp"
FakeWire Wire;
uint32_t activeMs = 0;
uint32_t millis() { return activeMs; }
void delay(uint32_t ms) { activeMs += ms; }
class Gas : public Loom_DFMultiGasSensor {
  public:
    Gas(Manager &manager, bool powersDown) : Loom_DFMultiGasSensor(manager, 0x74, 1, powersDown) {}
    using Loom_DFMultiGasSensor::initialize;
    using Loom_DFMultiGasSensor::power_up;
    bool available() const { return moduleInitialized; }
};
int main() {
    Manager manager;
    Gas retained(manager, false);
    retained.initialize();
    assert(retained.available());
    assert(gasFake::begins == 1 && gasFake::modes == 1 && gasFake::compensations == 1);
    for (int wake=0; wake<10; ++wake) { retained.power_up(); }
    assert(gasFake::begins == 1 && gasFake::modes == 1 && gasFake::compensations == 1);
    Wire.connected = false;
    retained.power_up();
    assert(!retained.available());
    Wire.connected = true;
    gasFake::modeAccepted = false;
    retained.power_up(); // A recovery must configure, and reject a missing acknowledgement.
    assert(!retained.available());
    assert(gasFake::modes == 2 && gasFake::compensations == 1);
    gasFake::modeAccepted = true;
    retained.power_up();
    assert(retained.available() && gasFake::modes == 3 && gasFake::compensations == 2);
    Gas cycled(manager, true);
    cycled.initialize();
    const unsigned int before = gasFake::modes;
    cycled.power_up();
    assert(cycled.available() && gasFake::modes == before + 1); // Power-cycle users retain setup.
}
