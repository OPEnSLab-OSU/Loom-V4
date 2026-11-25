#pragma once

#include "../I2CDevice.h"
#include "Loom_Manager.h"
#include <Wire.h>

#include <Adafruit_VCNL4030.h>

/**
 * Loom integration for VCNL4030 proximity and ambient light sensor.
 * Proximity = 16-bit counts, Lux = ambient light in lux, White = white channel counts.
 */
class Loom_VCNL4030 : public I2CDevice {
  protected:
    void initialize() override;
    void measure() override;
    void package() override;
    void power_up() override;
    void power_down() override {};

  public:
    // address 0x60 by default
    Loom_VCNL4030(Manager &managerInstance, int address = 0x60, bool useMux = false);

    uint16_t readProximity() { return proximity; }
    float readLux() { return lux; }
    uint16_t readWhite() { return white; }

  private:
    Manager *managerInstance;
    Adafruit_VCNL4030 vcnl;
    uint16_t proximity = 0;
    float lux = 0;
    uint16_t white = 0;
};
