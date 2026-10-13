#pragma once
#include <map>
#include <vector>

#include "Arduino.h"

#include "Module.h"
#include "Loom_Manager.h"


#include <SDI12.h>

#define RESPONSE_SIZE 81
#define IDENTIFICATION_RESPONSE_SIZE 36


/**
 * Provides both a loomified in addition to a standard reliable library implementation
 * 
 * @author Will Richards
 */ 
class Loom_SDI12 : public Module{
    protected:
        
        
        /* These should be called only by Manager.h */
        void measure() override;                                // Generic Measure Call To Pull Sensor Data
        void package() override;                                // Generic Package Call to Store Sensor Data
        void power_down() override;
        void power_up() override;

    public: 
        
        Loom_SDI12(Manager& man, const int pinAddr = 11);       // Loomified Constructor

        Loom_SDI12(const int pinAddr = 11);                     // Standard Sensor Interaction Constructor

        void initialize() override;                             // Initialize the sensor interface
        
        /* The following methods are intended for manual usage, outside of the Loom framework*/

        const char* getSensorInfo(char addr);                   // Get the info of the connected sensor
        void getInUseAddresses();                  // Get a list of the in use addresses

        void sendCommand(char* response, char addr, const char* command);     // Sends the given command to the given sensor on the bus and returns the result
        void requestSensorInfo(char* response, char addr);                    // Request Information about the connected SDI12 sensor
        void getData(char addr);                                // Get the data from the connected sensor
        void scanAddressSpace();                   // Scans over the SDI-12 address space and returns a list of in-use addresses

        float getTemperature() { return sensorData[0]; };       // Temperature of the soil
        float getDielectricPerm() { return sensorData[1]; };    // Dielectric Permittivity of the soil
        float getConductivity() { return sensorData[2]; };      // Conductivity of the soil

    private:
        Manager* manInst;                                       // Instance of the Manager

        SDI12 sdiInterface;                                     // SDI-12 Library Interface

        void readResponse(char response[RESPONSE_SIZE]);                   // Reads and returns the sensor's response to the command
        bool checkActive(char addr);                            // Checks if the current address is actually being used

        struct SensorInfo{
            char address;
            // <addr>I! will have a field with 7 char maximum sensor name
            char sensor_name[7];
            // <addr>M! will send up to 9 measurements in compliance with SDI-12 v1.4 standard
            float measurements[9];
            // varies by sensor. Response after sending <addr>M! will be 
            // <addr><wait time><measurement count>, which looks something like 00013 - address 0, 1 second, 3 measurements
            int wait_time;
        };

        size_t num_sensors = 0;
        // arbitrary limit on how many SDI-12 devices we can have connected at one time
        SensorInfo registered_sensors[10];



        
};
