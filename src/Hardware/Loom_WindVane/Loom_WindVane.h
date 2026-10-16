#pragma once

#include <stdint.h>

#include "Loom_Manager.h"
#include "Module.h"

/**
 * Reads the analog resistor ladder from a 16-point wind vane.
 * The direction is reported clockwise from North in degrees.
 */
class Loom_WindVane : public Module {
public:
    Loom_WindVane(Manager& man, uint8_t directionPin);

    void initialize() override;
    void measure() override;
    void package() override;
    void power_up() override {};
    void power_down() override {};

    float getWindDirectionDegrees() const { return windDirectionDegrees; }
    uint16_t getRawADC() const { return rawADC; }
    bool isDirectionValid() const { return directionValid; }

private:
    Manager* manInst;
    uint8_t directionPin;
    uint16_t rawADC = 0;
    float windDirectionDegrees = -1.0f;
    bool directionValid = false;
};
