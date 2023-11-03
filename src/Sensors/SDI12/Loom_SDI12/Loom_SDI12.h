#pragma once
#include <map>
#include <vector>

#include "Arduino.h"

#include "Module.h"
#include "Loom_Manager.h"


#include <SDI12.h>

#define RESPONSE_SIZE 50


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
        std::vector<char> getInUseAddresses();                  // Get a list of the in use addresses

        void sendCommand(char response[RESPONSE_SIZE], char addr, const char* command);     // Sends the given command to the given sensor on the bus and returns the result
        void requestSensorInfo(char response[RESPONSE_SIZE], char addr);                    // Request Information about the connected SDI12 sensor
        void getData(char addr);                                // Get the data from the connected sensor
        std::vector<char> scanAddressSpace();                   // Scans over the SDI-12 address space and returns a list of in-use addresses

        float getTemperature() { return sensorData[0]; };       // Temperature of the soil
        float getDielectricPerm() { return sensorData[1]; };    // Dielectric Permittivity of the soil
        float getConductivity() { return sensorData[2]; };      // Conductivity of the soil

    private:
        Manager* manInst;                                       // Instance of the Manager

        SDI12 sdiInterface;                                     // SDI-12 Library Interface

        int sensorTracker = 0;                                  // If we have multiple SDI-12 sensors on one bus we need to distinguish them in the json so increment a counter per

        float sensorData[3];                                    // Array of floats to store sensor data
        std::vector<char> inUseAddresses;                       // List of address that have SDI_12 sensors connected
        std::vector<char*> sensorNames;                         // List of strings of sensor names

        std::map<char, const char*> addressToType;              // Maps an SDI12 device address to a device type

        void readResponse(char response[RESPONSE_SIZE]);                   // Reads and returns the sensor's response to the command
        bool checkActive(char addr);                            // Checks if the current address is actually being used
        
};
