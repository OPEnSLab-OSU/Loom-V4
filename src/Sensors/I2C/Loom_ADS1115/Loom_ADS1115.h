#pragma once

#include "Loom_WarningGuards.h"

LOOM_EXTERNAL_INCLUDE_BEGIN
#include <Adafruit_ADS1X15.h>
LOOM_EXTERNAL_INCLUDE_END

#include "../I2CDevice.h"
#include <math.h>

// Only the pointer/reference belongs in this header; packet operations live in the .cpp.
class Manager;

/**
 * Functionality for the ADS1115
 *
 * @author Will Richards
 */
class Loom_ADS1115 : public I2CDevice {
  protected:
    void power_down() override {};

  public:
    void initialize() override;
    void measure() override;
    void package() override;
    void power_up() override;

    /**
     *  Construct a new ADS1115
     *  @param man Reference to the manager
     *  @param useMux Whether or not to use the mux
     *  @param address I2C address to communicate over
     *  @param enable_analog If we want to read the analog data from the ADS1115
     *  @param enable_diff If we want to read the differential data from the sensor
     *  @param gain How much gain to apply to the readings.
     */
    Loom_ADS1115(Manager &man, byte address = ADS1X15_ADDRESS, bool useMux = false,
                 bool enable_analog = true, bool enable_diff = false,
                 adsGain_t gain = adsGain_t::GAIN_ONE);

    /**
     * Return the latest raw ADC count. Missing readings or invalid channel numbers return NaN.
     * @param pin Channel number 1-4: 1 means A0, 2 means A1, etc.
     */
    float getAnalog(int pin) {
        return pin >= 1 && pin <= 4 && (validAnalogMask & (1U << (pin - 1)))
                   ? static_cast<float>(analogData[pin - 1])
                   : NAN;
    };

    /**
     * Return the latest raw differential count, or NaN when unavailable.
     * @param pin Pair number: 1 means A0 minus A1; 2 means A2 minus A3.
     */
    float getDiff(int pin) {
        return pin >= 1 && pin <= 2 && (validDiffMask & (1U << (pin - 1)))
                   ? static_cast<float>(diffData[pin - 1])
                   : NAN;
    };

    // Choose channels before logging starts. Bit 0 selects A0, bit 1 A1, etc.
    // The default 0x0F keeps all four channels; 0x03 reads only A0/A1. Zero disables all analogs.
    bool setAnalogChannelMask(uint8_t mask);
    // Keep raw counts and optionally omit the duplicate voltage columns. Defaults to true.
    void setOutputVoltages(bool enabled) { outputVoltages = enabled; };

  private:
    Manager *manInst;     // Instance of the manager
    Adafruit_ADS1115 ads; // Gain/voltage conversion only; no heap-allocating begin() call

    adsGain_t adc_gain; // Gain to set the amplifier to
    byte i2c_address;   // I2C address
    bool enableAnalog;  // Read from the analog pins
    bool enableDiff;    // Read differentials

    uint8_t analogChannelMask = 0x0F;
    uint8_t validAnalogMask =
        0; // Validity is separate from signed ADC values: every int16 is usable.
    uint8_t validDiffMask = 0;
    bool outputVoltages = true;

    bool writeRegister(uint8_t reg, uint16_t value);
    bool readRegister(uint8_t reg, uint16_t &value);
    bool readConversion(uint16_t mux, int16_t &value);

    int16_t analogData[4] = {}; // Stores the analog ADS1115 data
    int16_t diffData[2] = {};   // Stores the differential data from the sensor
    float volts[4] = {};        // Stores Computed Voltage Conversions
};
