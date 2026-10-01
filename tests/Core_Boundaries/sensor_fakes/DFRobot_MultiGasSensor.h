#pragma once
#include "Wire.h"
#include <cstring>
using TwoWire = FakeWire;
constexpr uint8_t CMD_GET_ALL_DTTA = 1, CMD_GET_GAS_CONCENTRATION = 2;
namespace gasFake {
static bool connected = true, modeAccepted = true;
static unsigned int begins = 0, modes = 0, compensations = 0;
}
// Counts configuration calls; does not model lower-core I2C stalls or electrical faults.
class DFRobot_GAS {
  public:
    enum eMethod_t { PASSIVITY, INITIATIVE };
    enum eSwitch_t { OFF, ON };
    enum GasType { O2, CO, H2S, NO2, O3, CL2, NH3, H2, HCL, SO2, HF, _PH3 };
    struct sProtocol_t { uint8_t command; };
    virtual ~DFRobot_GAS() = default;
    virtual bool dataIsAvailable() { return true; }
    bool begin() { ++gasFake::begins; return gasFake::connected; }
    bool changeAcquireMode(eMethod_t) { ++gasFake::modes; return gasFake::modeAccepted; }
    void setTempCompensation(eSwitch_t) { ++gasFake::compensations; }
    float readGasConcentrationPPM() { return 0; }
    float readTempC() { return 25; }
  protected:
    uint8_t command = 0;
    sProtocol_t pack(uint8_t *data, size_t) { return {data[0]}; }
    void writeData(uint8_t, void *data, size_t) { command = static_cast<sProtocol_t *>(data)->command; }
    int readData(uint8_t, uint8_t *data, size_t count) {
        std::memset(data, 0, count); data[0] = 0xFF; data[1] = command; data[4] = CO;
        uint8_t sum = 0;
        for (int i=1; i<7; ++i) { sum += data[i]; }
        data[8] = static_cast<uint8_t>(~sum + 1);
        return static_cast<int>(count);
    }
};
class DFRobot_GAS_I2C : public DFRobot_GAS {
  public:
    DFRobot_GAS_I2C(TwoWire *, uint8_t) {}
};
