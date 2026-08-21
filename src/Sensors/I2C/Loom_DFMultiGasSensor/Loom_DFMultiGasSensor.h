#pragma once

#include <DFRobot_MultiGasSensor.h>
#include <string.h>
#include <unordered_map>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

/**
 * Implementation of the DFRobot MultiGasSensor
 *
 * @author Will Richards, Soren Emmons
 */
class Loom_DFMultiGasSensor : public I2CDevice {
  protected:
    // Manager controlled functions
    void measure() override;
    void initialize() override;
    void power_up() override;
    void power_down() override {};
    void package() override;

  public:
    /**
     * Constructs a new sensor
     * @param man Reference to the manager that is used to universally package all data
     * @param address I2C address that is assigned to the sensor
     * @param initializationRetyLimit How many attempts will be made to initialize the sensor before
     * deeming it a failure and moving on
     * @param sensorPowersDown Does the sensor power down during sleep and need to re connect each
     * power_up
     * @param useMux Is this I2C device being multiplexed
     */
    Loom_DFMultiGasSensor(Manager &man, uint8_t address = 0x74,
                          uint8_t initializationRetyLimit = 10, bool sensorPowersDown = false,
                          bool useMux = false);

    /**
     * Get gas type that is currently being recorded
     */
    const char *getGasType() { return currentGasType; };

    /**
     * Get the gas concentration that is currently being recorded
     */
    float getGasConcentration() { return currentConcentration; };

    /**
     * Get the current temperature at the sensor location
     */
    float getTemperatureC() { return currentTemperature; };

  private:
    // --- Framework Properties ---
    Manager *manInst = nullptr;

    // --- Sensor Properties ---
    DFRobot_GAS_I2C gasSensor;
    uint8_t retryLimit;

    bool powersDown;

    bool attemptConnectionToSensor();
    void
    configureSensorProperties(DFRobot_GAS::eMethod_t aquireMode = DFRobot_GAS::eMethod_t::PASSIVITY,
                              DFRobot_GAS::eSwitch_t gasCompMode = DFRobot_GAS::eSwitch_t::ON);

    // --- Sensor Readings
    // Strinified reading of the current gas type
    const char *currentGasType = "";
    float currentConcentration = 0.0f;
    float currentTemperature = 0.0f;
};

uint8_t findGasBoard(void);