#pragma once

#include "../I2CDevice.h"
#include "Loom_Manager.h"

#include <Wire.h>
#include <MPU6050_tockn.h>

/**
 *  MPU6050 Accelerometer / IMU
 * 
 *  @author Will Richards
 */ 
class Loom_MPU6050 : public I2CDevice{
    protected:
        
        void power_up() override {};
        void power_down() override {}; 

    public:
        Loom_MPU6050(Manager& man, bool useMux = false, const bool autoCalibrate = true);

        void initialize() override;
        void measure() override;
        void package() override;

        /**
         * Manually re-calibrate the gyro
         */ 
        void calibrate();

    private:
        Manager* manInst;       // Instance of the manager
        MPU6050 mpu;            // Instance of the MPU sensor library
        bool autoCali;          // Whether or not to auto calibrate on startup

        float acc[3];           // Acceleration of the gyroscope
        float rate[3];          // Rate of rotation in degrees/second
        float angle[3];         // Angle of the gyroscope
};