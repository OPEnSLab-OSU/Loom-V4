#include "Loom_ADS1115.h"
#include "Logger.h"
#include "Loom_Manager.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ADS1115::Loom_ADS1115(Manager &man, byte address, bool useMux, bool enable_analog,
                           bool enable_diff, adsGain_t gain)
    : I2CDevice("ADS1115"), manInst(&man), adc_gain(gain), i2c_address(address),
      enableAnalog(enable_analog), enableDiff(enable_diff) {
    module_address = i2c_address;

    if (!useMux) {
        manInst->registerModule(this);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::initialize() {
    // Own the I2C master setup instead of depending on another module having
    // initialized Wire first. This is especially important for SmartRock,
    // where ADS1115 is the first registered I2C sensor.
    Wire.begin();

    // Probe without calling Adafruit_ADS1X15::begin(), which allocates its I2C
    // helper. This gives a newly enabled sensor a few bounded chances to become
    // ready without leaking memory on each attempt.
    uint8_t i2cStatus = 4;
    for (uint8_t attempt = 0; attempt < 3; attempt++) {
        Wire.beginTransmission(i2c_address);
        i2cStatus = Wire.endTransmission();
        if (i2cStatus == 0) {
            break;
        }
        delay(100);
    }

    // ADS1115 permits four addresses depending on how ADDR is strapped. If the
    // requested address is absent, find a responding ADS address so deployed
    // SmartRock boards do not depend on one particular strap configuration.
    if (i2cStatus != 0) {
        for (uint8_t candidate = 0x48; candidate <= 0x4B; candidate++) {
            if (candidate == i2c_address) {
                continue;
            }

            Wire.beginTransmission(candidate);
            if (Wire.endTransmission() == 0) {
                WARNINGF("ADS1115 did not answer at 0x%02X; using responding address 0x%02X.",
                         static_cast<unsigned>(i2c_address), static_cast<unsigned>(candidate));
                i2c_address = candidate;
                module_address = candidate;
                i2cStatus = 0;
                break;
            }
        }
    }

    if (i2cStatus != 0) {
        ERRORF("ADS1115 did not acknowledge at any address from 0x48 through 0x4B "
               "(configured address 0x%02X returned I2C status %u).",
               static_cast<unsigned>(i2c_address), static_cast<unsigned>(i2cStatus));
        moduleInitialized = false;
        return;
    }

    // Our checked register transfers use Wire directly. Keep Adafruit's gain/voltage maths,
    // but never allocate its I2C helper: repeated initialize() no longer leaks on reconnection.
    ads.setGain(adc_gain);
    moduleInitialized = true;
    needsReinit = false;
    LOG(F("Successfully initialized sensor!"));
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::measure() {
    validAnalogMask = 0;
    validDiffMask = 0; // Disconnects/timeouts must not package last cycle's counts as new readings.
    if (!moduleInitialized) {
        return;
    }

    const bool connectionStatus = checkDeviceConnection();
    if (!connectionStatus) {
        ERROR(F("No acknowledge received from the ADS1115"));
        return;
    }
    if (needsReinit) {
        initialize();
        if (!moduleInitialized) {
            return;
        }
    }

    if (enableAnalog) {
        for (uint8_t channel = 0; channel < 4; ++channel) {
            const uint8_t bit = static_cast<uint8_t>(1U << channel);
            if ((analogChannelMask & bit) == 0) {
                continue;
            }
            if (readConversion(MUX_BY_CHANNEL[channel], analogData[channel])) {
                volts[channel] = ads.computeVolts(analogData[channel]);
                validAnalogMask |= bit;
            } else {
                ERRORF("ADS1115 A%u conversion failed; value is missing this cycle.", channel);
            }
        }
    }

    // Differential readings are independent: enable_diff=true must work with enable_analog=false.
    if (enableDiff) {
        constexpr uint16_t differentialMux[] = {ADS1X15_REG_CONFIG_MUX_DIFF_0_1,
                                                ADS1X15_REG_CONFIG_MUX_DIFF_2_3};
        for (uint8_t channel = 0; channel < 2; ++channel) {
            if (readConversion(differentialMux[channel], diffData[channel])) {
                validDiffMask |= static_cast<uint8_t>(1U << channel);
            } else {
                ERRORF("ADS1115 differential pair %u conversion failed; value is missing.",
                       channel);
            }
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::package() {
    if (!moduleInitialized) {
        return;
    }
    JsonObject json = manInst->get_data_object(getModuleName());
    if (enableAnalog) {
        constexpr const char *rawKeys[] = {"A0", "A1", "A2", "A3"};
        constexpr const char *voltageKeys[] = {"A0_Volts", "A1_Volts", "A2_Volts", "A3_Volts"};
        // Two passes retain the established CSV order: all raw columns, then all voltages.
        for (uint8_t channel = 0; channel < 4; ++channel) {
            if ((analogChannelMask & (1U << channel)) != 0) {
                if ((validAnalogMask & (1U << channel)) != 0) {
                    json[rawKeys[channel]] = analogData[channel];
                } else {
                    json[rawKeys[channel]] = nullptr;
                }
            }
        }
        if (outputVoltages) {
            for (uint8_t channel = 0; channel < 4; ++channel) {
                if ((analogChannelMask & (1U << channel)) != 0) {
                    if ((validAnalogMask & (1U << channel)) != 0) {
                        json[voltageKeys[channel]] = volts[channel];
                    } else {
                        json[voltageKeys[channel]] = nullptr;
                    }
                }
            }
        }
    }
    if (enableDiff) {
        constexpr const char *keys[] = {"Diff_0", "Diff_1"};
        for (uint8_t channel = 0; channel < 2; ++channel) {
            if ((validDiffMask & (1U << channel)) != 0) {
                json[keys[channel]] = diffData[channel];
            } else {
                json[keys[channel]] = nullptr;
            }
        }
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::power_up() {

    // Restore the configured gain without calling begin() again. The packaged
    // Adafruit driver allocates its I2C helper in begin(), so repeatedly calling
    // it after every Hypnos wake would leak memory.
    if (moduleInitialized) {
        ads.setGain(adc_gain);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_ADS1115::setAnalogChannelMask(uint8_t mask) {
    if ((mask & 0xF0U) != 0) {
        return false;
    }
    analogChannelMask = mask;
    validAnalogMask = 0; // New channels need fresh measurements, even if toggled between cycles.
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_ADS1115::writeRegister(uint8_t reg, uint16_t value) {
    Wire.beginTransmission(i2c_address);
    const bool queued = Wire.write(reg) == 1 && Wire.write(static_cast<uint8_t>(value >> 8)) == 1 &&
                        Wire.write(static_cast<uint8_t>(value)) == 1;
    const uint8_t status = Wire.endTransmission();
    return queued && status == 0;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_ADS1115::readRegister(uint8_t reg, uint16_t &value) {
    Wire.beginTransmission(i2c_address);
    const bool queued = Wire.write(reg) == 1;
    if (Wire.endTransmission(false) != 0 || !queued) {
        return false;
    }
    if (Wire.requestFrom(i2c_address, static_cast<uint8_t>(2)) != 2) {
        return false;
    }
    const int high = Wire.read();
    const int low = Wire.read();
    if (high < 0 || low < 0) {
        return false;
    }
    value = static_cast<uint16_t>((static_cast<uint16_t>(high) << 8) | static_cast<uint16_t>(low));
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_ADS1115::readConversion(uint16_t mux, int16_t &value) {
    uint16_t status = 0;
    if (!readRegister(ADS1X15_REG_POINTER_CONFIG, status) ||
        (status & ADS1X15_REG_CONFIG_OS_MASK) == 0) {
        return false; // Starting while busy can be ignored; never call the old result a new sample.
    }
    const uint16_t config = ADS1X15_REG_CONFIG_OS_SINGLE | ADS1X15_REG_CONFIG_MODE_SINGLE |
                            RATE_ADS1115_128SPS | static_cast<uint16_t>(adc_gain) | mux |
                            ADS1X15_REG_CONFIG_CQUE_1CONV;
    // Retain the Adafruit driver's ALERT/RDY behavior while checking every transfer's result.
    if (!writeRegister(ADS1X15_REG_POINTER_CONFIG, config) ||
        !writeRegister(ADS1X15_REG_POINTER_HITHRESH, 0x8000) ||
        !writeRegister(ADS1X15_REG_POINTER_LOWTHRESH, 0x0000)) {
        return false;
    }

    // TI specifies 1/data-rate conversion time, +/-10% rate and about 25 us single-shot startup.
    // At 128 SPS, 1/(128*0.9) + 25 us is about 8.71 ms. 25 ms is our fault timeout with margin.
    // Read the OS bit to finish early; Adafruit's blocking reader waits forever on a bad device.
    const uint32_t started = millis();
    while (static_cast<uint32_t>(millis() - started) < 25) {
        if (!readRegister(ADS1X15_REG_POINTER_CONFIG, status) ||
            (status & ~ADS1X15_REG_CONFIG_OS_MASK) != (config & ~ADS1X15_REG_CONFIG_OS_MASK)) {
            return false;
        }
        if ((status & ADS1X15_REG_CONFIG_OS_MASK) != 0) {
            uint16_t raw = 0;
            if (!readRegister(ADS1X15_REG_POINTER_CONVERT, raw)) {
                return false;
            }
            // Decode two's complement without an out-of-range unsigned-to-signed cast.
            const int32_t signedValue = raw <= INT16_MAX ? raw : static_cast<int32_t>(raw) - 65536;
            value = static_cast<int16_t>(signedValue);
            return true;
        }
        delay(1);
    }
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
