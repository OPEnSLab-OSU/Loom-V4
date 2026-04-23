#pragma once

#include "../EZO/EZOSensor.h"
#include "Loom_Manager.h"

/**
 * Functionality for the EZO ORP Sensor
 *
 * @author Will Richards
 */
class Loom_EZOORP : public EZOSensor {
  protected:
    void power_up() override {};

  public:
    void initialize() override;
    void measure() override;
    void package() override;
    void power_down() override;

    /**
     *  Construct a new EZOORP device
     *  @param man Reference to the manager
     *  @param address I2C address to communicate over
     *  @param useMux Whether or not to use the mux
     */
    Loom_EZOORP(Manager &man, byte address = 0x62, bool useMux = false);

    /**
     * Get the color values individually
     */
    float getORP() { return orp; };

  private:
    Manager *manInst; // Instance of the manager

    float orp; // RGB readings
};