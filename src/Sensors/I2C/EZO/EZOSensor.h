#pragma once

#include <Wire.h>

#include "../I2CDevice.h"
#include "Logger.h"

class EZOSensor : public I2CDevice {
  public:
    /* Construct a new EZO device */
    EZOSensor(const char *modName) : I2CDevice(modName){};

    /* General command to transmit data over I2C to the given device*/
    bool sendTransmission(const char *command) {
        Wire.beginTransmission(module_address);
        Wire.write(command);

        // Use a ternary operator to ensure if it is 0 its true if not we are false
        return Wire.endTransmission() == 0;
    };

    /* Calibrate The Device */
    bool calibrate() {
        // Send the calibrate command
        if (!sendTransmission("Cal")) {
            ERROR(F("Failed to transmit calibration command"));
            return false;
        }

        LOG(F("Calibrating Device..."));

        // Wait calibration time
        delay(1300);

        LOG(F("Device successfully calibrated!"));

        return true;
    };

    /**
     * Request and read in data from the senor
     *
     * @param waitTime This is the length of time we will wait between requesting a read and
     * actually reading the data
     * @return Whether or not the read was successfully
     * */
    bool readSensor(int waitTime) {
        if (!moduleInitialized) {
            return false;
        }

        // Clear the previous sample so a failed/short transaction cannot expose stale bytes.
        memset(sensorData, 0, sizeof(sensorData));

        if (!sendTransmission("r")) {
            ERROR(F("Failed to send 'read' command to device"));
            return false;
        }

        delay(waitTime);

        // EZO responses contain one status byte followed by a null-terminated payload. Copy each
        // byte directly and bound the read; a one-byte response character is not itself a C
        // string.
        const uint8_t received = Wire.requestFrom(module_address, 32, 1);
        if (received == 0 || Wire.available() < 1) {
            ERROR(F("No response received from EZO device"));
            return false;
        }

        const int responseCode = Wire.read();
        if (responseCode != 1) {
            ERRORF("Unsuccessful Response Code Received: %s", responseCodeText(responseCode));
            return false;
        }

        size_t length = 0;
        while (Wire.available() > 0 && length < sizeof(sensorData) - 1) {
            const int value = Wire.read();
            if (value < 0 || value == '\0') {
                break;
            }
            sensorData[length++] = static_cast<char>(value);
        }
        sensorData[length] = '\0';
        return length > 0;
    };

    /* Get the most recently collected sensor data */
    const char *getSensorData() { return sensorData; };

  private:
    static const char *responseCodeText(const int code) {
        switch (code) {
        case 1:
            return "Success";
        case 2:
            return "Failed";
        case 254:
            return "Pending";
        case 255:
            return "No Data";
        default:
            return "Unknown";
        }
    }

    char sensorData[33] = {}; // Null-terminated EZO payload
};
