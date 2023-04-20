#pragma once

#include <Wire.h>

#include "../I2CDevice.h"
#include "Logger.h"

class EZOSensor : public I2CDevice{
    public:

        /* Construct a new EZO device */
        EZOSensor(const char* modName) : I2CDevice(modName) {};

        
        /* General command to transmit data over I2C to the given device*/
        bool sendTransmission(const char* command){
            Wire.beginTransmission(module_address);
            Wire.write(command);

            // Use a ternary operator to ensure if it is 0 its true if not we are false
            return Wire.endTransmission() == 0;
        };

        /* Calibrate The Device */
        bool calibrate(){
            // Send the calibrate command
            if(!sendTransmission("Cal")){
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
            int i;
            if(moduleInitialized){
                // Clear the sensorData received previously
                memset(sensorData, '\0', 32);

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
                Serial.println(code);
                if(code != 1){
                    snprintf(output, OUTPUT_SIZE, "Unsuccessful Response Code Received: %s", responseCodes[code-1]);
                    ERROR(output);
                    return false;
                } 

                // Read out only the next 32 bytes
                for(i = 0; i < 32; i++){
                    currentChar = (char)Wire.read();

                    // If a null char was received break out of the loop
                    if(currentChar == '\0') break;

                    // If not append the current char to the string
                    strncat(sensorData, &currentChar, 32);
                }
                strncat(sensorData, "\0", 32);
            }

            return true;
        };

        /* Get the most recently collected sensor data */
        const char* getSensorData() { return sensorData; };
    
    private:
        int8_t code = 0;                                                            // I2C Response Code
        char currentChar;                                                           // Current character we have read in
        const char* responseCodes[4] = {"Success", "Failed", "Pending", "No Data"}; // Stringified I2C Response codes
        char sensorData[33];                                                        // Convert the char array to a string to improve parse-ability
        
};