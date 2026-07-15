#pragma once

#include <AS726X.h>
#include <Wire.h>

#include "../I2CDevice.h"
#include "Loom_Manager.h"

/**
 * AS7263 NIR sensor
 *
 * @author Will Richards
 */
class Loom_AS7263 : public I2CDevice {
  protected:
    void power_down() override {};

    // Manager controlled functions
    void measure() override;
    void initialize() override;
    void package() override;
    void power_up() override;

  public:
    /**
     * Constructs a new TSL2591 sensor
     * @param man Reference to the manager that is used to universally package all data
     * @param useMux If this module will be using the mux
     * @param address I2C address that is assigned to the sensor
     * @param gain Gain level
     * @param mode Read Mode: 0("4 channels out of 6"), 1("Different 4 channels out of 6"), 2("All 6
     * channels continuously"), 3("One-shot reading of all channels")
     * @param integration_time Integration time (time will be 2.8ms * [integration value])
     */
    Loom_AS7263(Manager &man, bool useMux = false, int addr = 0x49, uint8_t gain = 1,
                uint8_t mode = 3, uint8_t integration_time = 50);

  private:
    Manager *manInst; // Instance of the manager
    AS726X asInst;    // Instance of the AS7263

    uint16_t nir[6]; // Measured near-infra-red bands values. Units: counts / (μW/cm^2)

    uint8_t gain;             // Gain setting
    uint8_t mode;             // Sensor mode
    uint8_t integration_time; // Integration time setting
};