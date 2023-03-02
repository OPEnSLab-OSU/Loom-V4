#pragma once

#include <Wire.h>

#include "../I2CDevice.h"
#include "Logger.h"

class EZOSensor : public I2CDevice{
    protected:
        byte i2c_address; // Stores the I2C device of the EZO device
    public:

        /* Construct a new EZO device */
        EZOSensor(const char* modName) : I2CDevice(modName) {};

        
        /* General command to transmit data over I2C to the given device*/
        bool sendTransmission(String command){
            Wire.beginTransmission(module_address);
            Wire.write(command.c_str());

            // Use a ternary operator to ensure if it is 0 its true if not we are false
            return Wire.endTransmission() == 0 ? true : false;
        };

        /* Calibrate The Device */
        bool calibrate(){
            // Check the device is connected before calibrating
            if(!checkDeviceConnection()){
                ERROR(F("Failed to detect device at the specified address"));
                return false;
            }

            // Send the calibrate command
            if(sendTransmission("cal") != 0){
                ERROR(F("Failed to transmit calibration command"));
                return false;
            }

            LOG(F("Calibrating Device..."));

            // Wait calibration time
            delay(1300);

            LOG(F("Device successfully calibrated!"));

            return true;
        };

        /* Request and read in data from the senor*/
        bool readSensor(){
            char output[OUTPUT_SIZE];
            if(moduleInitialized){
                // Clear the sensorData received previously
                sensorData = "";

                // Check that the device is still present and connected
                if(!checkDeviceConnection()){
                    ERROR(F("Failed to detect device at the specified address"));
                    return false;
                }

                // Attempt to send a read command to the device
                if(!sendTransmission("r")){
                    ERROR(F("Failed to send 'read' command to device"));
                    return false;
                }
                
                // Wait the desired warm-up period
                delay(600);  

                // Request 32 bytes of data from the device
                Wire.requestFrom(module_address, 32, 1);

                // Check if the I2C code was not valid
                code = Wire.read();
                if(code != 1){
                    snprintf(output, OUTPUT_SIZE, "Unsuccessful Response Code Received: %s", responseCodes[code-1].c_str());
                    ERROR(output);
                    return false;
                } 

                // Read out only the next 32 bytes
                for(int i = 0; i < 32; i++){
                    currentChar = Wire.read();

                    // If a null char was received break out of the loop
                    if(currentChar == 0) break;

                    // If not append the current char to the string
                    sensorData += currentChar;
                }
            }

            return true;
        };

        /* Get the most recently collected sensor data */
        String getSensorData() { return sensorData; };
    
    private:
        int8_t code = 0;                                                        // I2C Response Code
        char currentChar;                                                       // Current character we have read in
        String responseCodes[4] = {"Success", "Failed", "Pending", "No Data"};  // Stringified I2C Response codes
        String sensorData;                                                      // Convert the char array to a string to improve parse-ability
        
};