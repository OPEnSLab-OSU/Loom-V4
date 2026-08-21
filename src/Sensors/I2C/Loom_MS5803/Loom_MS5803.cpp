#include "Loom_MS5803.h"
#include "Logger.h"

namespace {

bool readMS5803PromWord(const byte address, const byte command, uint16_t &value) {
    Wire.beginTransmission(address);
    Wire.write(command);
    if (Wire.endTransmission() != 0) {
        return false;
    }

    const uint8_t received = Wire.requestFrom(address, static_cast<uint8_t>(2));
    if (received != 2 || Wire.available() < 2) {
        return false;
    }

    value = (static_cast<uint16_t>(Wire.read()) << 8) | Wire.read();
    return true;
}

bool probeMS5803(const byte address) {
    // First verify that something acknowledges at the requested address.
    Wire.beginTransmission(address);
    if (Wire.endTransmission() != 0) {
        return false;
    }

    // Reset command from the MS5803 command set.
    Wire.beginTransmission(address);
    Wire.write(0x1E);
    if (Wire.endTransmission() != 0) {
        return false;
    }
    delay(5);

    // A real MS5803 should expose several non-empty calibration coefficients.
    uint8_t validWords = 0;
    for (byte command = 0xA2; command <= 0xAC; command += 2) {
        uint16_t coefficient = 0;
        if (!readMS5803PromWord(address, command, coefficient)) {
            return false;
        }
        if (coefficient != 0x0000 && coefficient != 0xFFFF) {
            validWords++;
        }
    }

    return validWords >= 4;
}

} // namespace

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MS5803::Loom_MS5803(Manager &man, byte address, bool useMux)
    : I2CDevice("MS5803"), manInst(&man), inst(address, 512) {
    module_address = address;

    if (!useMux)
        manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::initialize() {
    FUNCTION_START;

    // The legacy MS5803_02 library has historically returned an unreliable status from
    // initializeMS_5803(). Determine presence from the actual I2C device and calibration PROM
    // instead of permanently disabling the Loom module based on that return value.
    if (!probeMS5803(static_cast<byte>(module_address))) {
        ERRORF("MS5803 not detected or PROM invalid at address 0x%02X.",
               static_cast<unsigned>(module_address));
        moduleInitialized = false;
        FUNCTION_END;
        return;
    }

    // Initialize the external library, but deliberately do not trust its boolean result.
    inst.initializeMS_5803(false);
    delay(1000);

    // Prime the first conversion so getters do not remain at their zero-initialized defaults.
    inst.readSensor();
    sensorData[0] = inst.temperature();
    sensorData[1] = inst.pressure();

    moduleInitialized = true;
    needsReinit = false;

    LOG(F("Successfully initialized MS5803 at address: "));
    Serial.print(F("0x"));
    Serial.println(module_address, HEX);

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::measure() {
    FUNCTION_START;

    if (!moduleInitialized) {
        // Allow recovery after a temporary power or bus fault.
        initialize();
        if (!moduleInitialized) {
            FUNCTION_END;
            return;
        }
    }

    if (!checkDeviceConnection()) {
        ERROR(F("No acknowledge received from the MS5803"));
        moduleInitialized = false;
        FUNCTION_END;
        return;
    }

    // The legacy library expects initialization before a conversion. Ignore its unreliable
    // return status and validate the physical device through the I2C checks above.
    inst.initializeMS_5803(false);
    delay(1000);

    inst.readSensor();
    sensorData[0] = inst.temperature();
    sensorData[1] = inst.pressure();

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::package() {
    FUNCTION_START;
    if (moduleInitialized) {
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Temperature_°C"] = sensorData[0];
        json["Pressure_mbar"] = sensorData[1];
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::power_up() {
    FUNCTION_START;
    initialize();
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
