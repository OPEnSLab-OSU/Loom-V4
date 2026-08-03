#include "Loom_ADS1115.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ADS1115::Loom_ADS1115(Manager &man, byte address, bool useMux, bool enable_analog,
                           bool enable_diff, adsGain_t gain)
    : I2CDevice("ADS1115"), manInst(&man), adc_gain(gain), i2c_address(address),
      enableAnalog(enable_analog), enableDiff(enable_diff) {
    module_address = i2c_address;

    if (!useMux)
        manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
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
        if (i2cStatus == 0)
            break;
        delay(100);
    }

    // ADS1115 permits four addresses depending on how ADDR is strapped. If the
    // requested address is absent, find a responding ADS address so deployed
    // SmartRock boards do not depend on one particular strap configuration.
    if (i2cStatus != 0) {
        for (uint8_t candidate = 0x48; candidate <= 0x4B; candidate++) {
            if (candidate == i2c_address)
                continue;

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

    if (!ads.begin(i2c_address)) {
        ERROR(F("Failed to initialize ADS1115 interface! Data may be invalid"));
        moduleInitialized = false;
    } else {
        moduleInitialized = true;
        needsReinit = false;
        LOG(F("Successfully initialized sensor!"));
    }

    // Set the gain of the ADC
    ads.setGain(adc_gain);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::measure() {
    if (moduleInitialized) {
        // Get the current connection status
        bool connectionStatus = checkDeviceConnection();

        // If we are connected and we need to reinit
        if (connectionStatus && needsReinit) {
            initialize();
            needsReinit = false;
        }

        // If we are not connected
        else if (!connectionStatus) {
            ERROR(F("No acknowledge received from the device"));
            return;
        }

        if (enableAnalog) {
            for (int i = 0; i < 4; i++) {
                analogData[i] = ads.readADC_SingleEnded(i);
                volts[i] = ads.computeVolts(analogData[i]);
            }

            if (enableDiff) {
                diffData[0] = (int)ads.readADC_Differential_0_1();
                diffData[1] = (int)ads.readADC_Differential_2_3();
            }
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::package() {
    if (moduleInitialized) {
        JsonObject json = manInst->get_data_object(getModuleName());
        if (enableAnalog) {
            json["A0"] = analogData[0];
            json["A1"] = analogData[1];
            json["A2"] = analogData[2];
            json["A3"] = analogData[3];

            json["A0_Volts"] = volts[0];
            json["A1_Volts"] = volts[1];
            json["A2_Volts"] = volts[2];
            json["A3_Volts"] = volts[3];
        }

        if (enableDiff) {
            json["Diff_0"] = diffData[0];
            json["Diff_1"] = diffData[1];
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::power_up() {

    // Restore the configured gain without calling begin() again. The packaged
    // Adafruit driver allocates its I2C helper in begin(), so repeatedly calling
    // it after every Hypnos wake would leak memory.
    if (moduleInitialized) {
        ads.setGain(adc_gain);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
