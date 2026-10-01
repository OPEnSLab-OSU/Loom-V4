#pragma once
#include "Wire.h"
struct SEN66DeviceStatus { uint32_t value = 0; };
namespace senFake {
static bool running = true;
static int16_t error = 0;
static unsigned int transfers = 0, readings = 0, resets = 0;
}
// Model sensor state/transfers, not electrical I2C timing. millis() is controlled by the test.
class SensirionI2cSen66 {
  public:
    void begin(FakeWire &, uint8_t) {}
    int16_t getDataReady(uint8_t &padding, bool &ready) {
        ++senFake::transfers; padding = 0; ready = senFake::running; return senFake::error;
    }
    int16_t stopMeasurement() { ++senFake::transfers; senFake::running = false; return 0; }
    int16_t deviceReset() { ++senFake::transfers; ++senFake::resets; return 0; }
    int16_t startContinuousMeasurement() { ++senFake::transfers; senFake::running = true; return 0; }
    int16_t readMeasuredValuesAsIntegers(uint16_t &a, uint16_t &b, uint16_t &c, uint16_t &d,
                                       int16_t &h, int16_t &t, int16_t &v, int16_t &n, uint16_t &co2) {
        ++senFake::transfers; ++senFake::readings;
        a=b=c=d=10; h=4000; t=5000; v=1000; n=10; co2=650; return 0;
    }
    int16_t readNumberConcentrationValuesAsIntegers(uint16_t &a, uint16_t &b, uint16_t &c,
                                                   uint16_t &d, uint16_t &e) {
        ++senFake::transfers; a=b=c=d=e=10; return 0;
    }
    int16_t readDeviceStatus(SEN66DeviceStatus &status) { ++senFake::transfers; status.value=0; return 0; }
    uint16_t setTemperatureOffsetParameters(int16_t, int16_t, uint16_t, uint16_t) { return 0; }
    uint16_t setVocAlgorithmTuningParameters(int16_t,int16_t,int16_t,int16_t,int16_t,int16_t) { return 0; }
    uint16_t setNoxAlgorithmTuningParameters(int16_t,int16_t,int16_t,int16_t,int16_t,int16_t) { return 0; }
};
