#pragma once

#include "Loom_WarningGuards.h"

#include <array>
#include <vector>

LOOM_EXTERNAL_INCLUDE_BEGIN
#include "Arduino.h"
LOOM_EXTERNAL_INCLUDE_END

#include "Module.h"
#include "Utilities/Loom_SDI12Utils.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <SDI12.h>
LOOM_EXTERNAL_INCLUDE_END

#define RESPONSE_SIZE 50

// Only the pointer/reference belongs in this header; packet operations live in the .cpp.
class Manager;

/**
 * Reads supported soil probes on one SDI-12 data wire, each with its own character address.
 * Use Manager for measure/package cycles, or initialize/getData and the getters for manual use.
 * The probe supplies its required measurement wait; a data request must follow that wait.
 *
 * @author Will Richards
 */
class Loom_SDI12 : public Module {
  protected:
    /* These should be called only by Manager.h */
    void measure() override; // Generic Measure Call To Pull Sensor Data
    void package() override; // Generic Package Call to Store Sensor Data
    void power_down() override;
    void power_up() override;

  public:
    Loom_SDI12(Manager &man, const int pinAddr = 11); // Loomified Constructor

    Loom_SDI12(const int pinAddr = 11); // Standard Sensor Interaction Constructor

    Loom_SDI12(const Loom_SDI12 &) = delete;
    Loom_SDI12 &operator=(const Loom_SDI12 &) = delete;

    void initialize() override; // Initialize the sensor interface

    /* The following methods are intended for manual usage, outside of the Loom framework*/

    const char *getSensorInfo(char addr);  // Get the info of the connected sensor
    std::vector<char> getInUseAddresses(); // Get a list of the in use addresses

    void sendCommand(char response[RESPONSE_SIZE], char addr,
                     const char *command); // Sends the given command to the given sensor on the bus
                                           // and returns the result
    void requestSensorInfo(char response[RESPONSE_SIZE],
                           char addr);    // Request Information about the connected SDI12 sensor
    void getData(char addr);              // Get the data from the connected sensor
    std::vector<char> scanAddressSpace(); // Scans over the SDI-12 address space and returns a list
                                          // of in-use addresses

    // Configure before initialize(). Startup is a deployment policy, not a guaranteed sensor
    // maximum.
    bool setPowerUpDelay(uint32_t milliseconds);
    bool setMaximumMeasurementWait(uint16_t seconds);

    // Depths are numbered 1-4 for TEROS 54; other supported sensors have temperature at depth 1.
    // An address is a character ('0', 'a', etc.), not a numeric pin or array index.
    // getWaterContent() returns calibrated m3/m3 for TEROS 54 only. Missing/unsupported is NaN.
    float getTemperature(char address, uint8_t depth = 1) const;
    float getWaterContent(char address, uint8_t depth = 1) const;
    float getMatricPotential(char address) const; // TEROS 21, in kPa

    float getTemperature() { return sensorData[0]; }; // Temperature of the soil
    // Legacy soil-value getter: GS3 permittivity, TER11/12 counts; use model-specific getters for
    // TER21 potential and TER54 calibrated water content.
    float getDielectricPerm() { return sensorData[1]; };
    float getConductivity() { return sensorData[2]; }; // Conductivity of the soil

  private:
    Manager *manInst = nullptr; // Instance of the Manager

    SDI12 sdiInterface; // SDI-12 Library Interface

    struct SensorRecord {
        char type[RESPONSE_SIZE] = {};
        char name[MODULE_NAME_SIZE] = {};
        // Temperature/value pairs per depth; slot 2 remains conductivity for GS3/TEROS 12.
        std::array<float, loomSDI12::MAX_VALUES> data;
        SensorRecord() { data.fill(NAN); }
    };

    float sensorData[3] = {NAN, NAN, NAN}; // Most recently read sensor data for the manual getters
    std::vector<char> inUseAddresses;      // List of address that have SDI_12 sensors connected
    std::vector<SensorRecord> sensors;     // Per-address type, name, and latest readings

    uint32_t powerUpDelayMs = 1500; // METER TEROS 54: 1000 ms typical; allow a startup margin.
    uint16_t maximumMeasurementWaitSeconds =
        30; // Reject unexpectedly long requests before waiting.

    void readResponse(
        char response[RESPONSE_SIZE]); // Reads and returns the sensor's response to the command
    bool checkActive(char addr);       // Checks if the current address is actually being used
    int findSensorIndex(char addr) const;
};
