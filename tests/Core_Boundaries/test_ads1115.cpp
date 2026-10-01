// Register-level tests of the production driver, using only fake hardware/Manager surfaces.
#include <assert.h>
#include <math.h>
#include <string>
#include "../../src/Sensors/I2C/Loom_ADS1115/Loom_ADS1115.cpp"

FakeWire Wire;
uint32_t now = 0;
uint32_t millis() { return now; }
void delay(uint32_t ms) { now += ms; }

void resetBus() {
    Wire = FakeWire();
    now = 0;
}
std::string packet(Manager &manager) {
    std::string result;
    serializeJson(manager.document, result);
    return result;
}
int main() {
    resetBus();
    Manager manager;
    Loom_ADS1115 sensor(manager);
    sensor.initialize();
    Wire.samples = {INT16_MIN, -1, 0, INT16_MAX};
    Wire.busyReads = 9;
    sensor.measure();
    sensor.package();
    assert(sensor.getAnalog(1) == INT16_MIN && sensor.getAnalog(4) == INT16_MAX);
    assert(isnan(sensor.getAnalog(0)) && isnan(sensor.getAnalog(5)));
    assert(Wire.conversions == 4 && now == 36);
    const std::string defaultPacket = packet(manager);
    assert(defaultPacket.find("\"A3\"") < defaultPacket.find("\"A0_Volts\""));
    assert(manager.document["ADS1115"]["A0"].as<int>() == INT16_MIN);

    // An invalid mask is rejected without altering the old selection; non-adjacent works.
    assert(!sensor.setAnalogChannelMask(0x80));
    assert(sensor.setAnalogChannelMask(0x05));
    sensor.setOutputVoltages(false);
    Wire.samples = {100, 200};
    Wire.sample = 0;
    manager.document.clear();
    sensor.measure();
    sensor.package();
    assert(packet(manager) == "{\"ADS1115\":{\"A0\":100,\"A2\":200}}");
    assert(isnan(sensor.getAnalog(2)));

    resetBus();
    Manager diffManager;
    Loom_ADS1115 differential(diffManager, 0x48, false, false, true);
    differential.initialize();
    Wire.samples = {INT16_MIN, INT16_MAX};
    differential.measure();
    differential.package();
    assert(packet(diffManager) == "{\"ADS1115\":{\"Diff_0\":-32768,\"Diff_1\":32767}}");
    assert(isnan(differential.getAnalog(1)) && isnan(differential.getDiff(3)));

    // Inject each queued-byte, transfer-status and read-length failure for one conversion.
    for (int faultKind = 0; faultKind < 3; ++faultKind) {
        const size_t faultCount = faultKind == 0 ? 12 : faultKind == 1 ? 7 : 3;
        for (size_t offset = 0; offset < faultCount; ++offset) {
            resetBus();
            Manager failedManager;
            Loom_ADS1115 failing(failedManager);
            failing.initialize();
            assert(failing.setAnalogChannelMask(1));
            failing.setOutputVoltages(false);
            Wire.writeCalls = Wire.endCalls = Wire.requestCalls = 0;
            if (faultKind == 0) {
                Wire.failWrite = offset;
            }
            if (faultKind == 1) {
                Wire.failEnd = offset;
            }
            if (faultKind == 2) {
                Wire.failRequest = offset;
            }
            failing.measure();
            failing.package();
            assert(isnan(failing.getAnalog(1)));
            assert(failedManager.document["ADS1115"]["A0"].isNull());
        }
    }

    // Timeout stays finite across timer rollover; configuration mismatch also rejects data.
    for (bool mismatch : {false, true}) {
        resetBus();
        Manager failedManager;
        Loom_ADS1115 failing(failedManager);
        failing.initialize();
        failing.setAnalogChannelMask(1);
        Wire.neverReady = !mismatch;
        Wire.mismatch = mismatch;
        now = UINT32_MAX - 10;
        const uint32_t start = now;
        failing.measure();
        assert(isnan(failing.getAnalog(1)));
        assert(static_cast<uint32_t>(now - start) == (mismatch ? 0U : 25U));
    }

    // A disconnected cycle clears prior valid counts; it cannot package an old sample.
    resetBus();
    Manager unpluggedManager;
    Loom_ADS1115 unplugged(unpluggedManager);
    unplugged.initialize();
    unplugged.setAnalogChannelMask(1);
    Wire.samples = {123};
    unplugged.measure();
    assert(unplugged.getAnalog(1) == 123);
    Wire.connected = false;
    unplugged.measure();
    unplugged.package();
    assert(isnan(unplugged.getAnalog(1)));
    assert(unpluggedManager.document["ADS1115"]["A0"].isNull());
}
