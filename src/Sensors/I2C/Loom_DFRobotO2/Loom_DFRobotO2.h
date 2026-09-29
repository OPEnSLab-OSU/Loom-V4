#pragma once

#include "Loom_WarningGuards.h"

#include "../I2CDevice.h"
#include "Loom_Manager.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <DFRobot_OxygenSensor.h>
#include <Wire.h>
LOOM_EXTERNAL_INCLUDE_END
/**
 *  DFRobot Oxygen Sensor
 *
 *  @author Sarvesh Thiruppathi Ahila
 */
class Loom_DFRobotO2 : public I2CDevice {
  protected:
    void power_up() override {};
    void power_down() override {};

  public:
    Loom_DFRobotO2(Manager &man, bool useMux = false, int address = 0x73, int collectNum = 10);

    void initialize() override;
    void measure() override;
    void package() override;

    /**
     * Manually re-calibrate the sensor
     */
    void calibrate(float calOxygenConcentration, float calOxygenMV);

  private:
    Manager *manInst;            // Instance of the manager
    DFRobot_OxygenSensor oxygen; // Instance of the DFRobot Oxygen sensor library
    int collectNumber;           // Number of data points to collect; default is 10

    float oxygenConcentration = 0.0f; // Oxygen Concentration
};
