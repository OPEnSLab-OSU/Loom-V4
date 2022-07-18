#pragma once

#include <Adafruit_SHT31.h>

#include "Module.h"
#include "Loom_Manager.h"

class Loom_SHT31 : public Module{
    protected:
       
       // Manager controlled functions
        void measure() override;                               
        void print_measurements() override;
        void initialize() override;    
        void power_up() override {};
        void power_down() override {}; 
        void package() override;   

    public:
        /**
         * Constructs a new TSL2591 sensor
         * @param man Reference to the manager that is used to universally package all data
         * @param address I2C address that is assigned to the sensor
         */ 
        Loom_SHT31(
                      Manager& man, 
                      int address = 0x44 
                );


        /**
         * Get the humidity reading
         */ 
        float getHumidity() { return sensorData[1]; };

        /**
         * Get the temperature reading
         */ 
        float getTemperature() { return sensorData[0]; };

    private:
        Manager* manInst;                           // Instance of the manager
        Adafruit_SHT31 sht;                         // Adafruit TSL2591 Sensor Object

        int i2c_address;                            // I2C address of the device

        float sensorData[2] = {0, 0};               // Array of size 2 to hold the temp and humidity data

        bool initialized = true;
};