#pragma once
extern float testBatteryVoltage;
class Loom_Analog {
  public:
    static float getBatteryVoltage() { return testBatteryVoltage; }
};
