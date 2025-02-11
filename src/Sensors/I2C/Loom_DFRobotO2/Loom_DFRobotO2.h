#pragma once

#include "../I2CDevice.h"
#include "Loom_Manager.h"

#include <Wire.h>
#include <DFRobot_OxygenSensor.h>
/**
 *  DFRobot Oxygen Sensor
 * 
 *  @author Sarvesh Thiruppathi Ahila 
 */ 
class Loom_DFRobotO2 : public I2CDevice{
    protected:
        
        void power_up() override {};
        void power_down() override {}; 

    public:
        Loom_DFRobotO2(Manager& man, bool useMux = false, int address = 0x73, int collectNum = 10);

        void initialize() override;
        void measure() override;
        void package() override;

        /**
         * Manually re-calibrate the sensor
         */ 
        void calibrate(float calOxygenConcentration, float calOxygenMV);

    private:
        Manager* manInst;               // Instance of the manager
        DFRobot_OxygenSensor oxygen;    // Instance of the DFRobot Oxygen sensor library
        int collectNumber;               // Number of data points to collect; default is 10

        float oxygenConcentration; // Acceleration of the gyroscope
};