#include "Loom_WarningGuards.h"

#include "Loom_DFMultiGasSensor.h"
#include "Logger.h"
LOOM_EXTERNAL_INCLUDE_BEGIN
#include "Wire.h"
LOOM_EXTERNAL_INCLUDE_END

namespace {
const char *gasTypeText(uint8_t type) {
    switch (type) {
    case DFRobot_GAS::O2:
        return "O2";
    case DFRobot_GAS::CO:
        return "CO";
    case DFRobot_GAS::H2S:
        return "H2S";
    case DFRobot_GAS::NO2:
        return "NO2";
    case DFRobot_GAS::O3:
        return "O3";
    case DFRobot_GAS::CL2:
        return "CL2";
    case DFRobot_GAS::NH3:
        return "NH3";
    case DFRobot_GAS::H2:
        return "H2";
    case DFRobot_GAS::HCL:
        return "HCL";
    case DFRobot_GAS::SO2:
        return "SO2";
    case DFRobot_GAS::HF:
        return "HF";
    case DFRobot_GAS::_PH3:
        return "PH3";
    default:
        return "";
    }
}
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_DFGasI2C::hasValidResponse(const uint8_t response[9], uint8_t command) {
    // Match the checksum used by the packaged DFRobot sensor firmware/library. It skips the 0xFF
    // header and covers the same six response bytes as the vendor implementation.
    uint8_t checksum = 0;
    for (uint8_t index = 1; index < 7; ++index) {
        checksum += response[index];
    }
    checksum = static_cast<uint8_t>(~checksum + 1);
    return response[0] == 0xFF && response[1] == command && response[8] == checksum;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_DFGasI2C::dataIsAvailable() {
    uint8_t request[6] = {CMD_GET_ALL_DTTA, 0, 0, 0, 0, 0};
    uint8_t response[9] = {};
    sProtocol_t protocol = pack(request, sizeof(request));
    writeData(0, &protocol, sizeof(protocol));
    delay(10);
    readData(0, response, sizeof(response));
    return hasValidResponse(response, CMD_GET_ALL_DTTA);
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
const char *Loom_DFGasI2C::queryGasTypeFixed() {
    uint8_t request[6] = {CMD_GET_GAS_CONCENTRATION, 0, 0, 0, 0, 0};
    uint8_t response[9] = {};
    sProtocol_t protocol = pack(request, sizeof(request));
    writeData(0, &protocol, sizeof(protocol));
    delay(10);
    readData(0, response, sizeof(response));
    return hasValidResponse(response, CMD_GET_GAS_CONCENTRATION) ? gasTypeText(response[4])
                                                                 : "NO GAS";
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_DFMultiGasSensor::Loom_DFMultiGasSensor(Manager &man, uint8_t address,
                                             uint8_t initializationRetyLimit, bool sensorPowersDown,
                                             bool useMux)
    : I2CDevice("DFR_MultiGasSensor"), manInst(&man), gasSensor(&Wire, address),
      retryLimit(initializationRetyLimit > 0 ? initializationRetyLimit : 1),
      powersDown(sensorPowersDown) {
    module_address = address;

    // Register the module with the manager
    if (!useMux) {
        manInst->registerModule(this);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFMultiGasSensor::initialize() {
    FUNCTION_START(this);

    LOG(F("Begin DFRobot Multi Gas Sensor Initialization..."));

    Wire.begin();
    if (!checkDeviceConnection()) {
        ERRORF("No gas board found at configured address 0x%02X.", module_address);
        moduleInitialized = false;
        return;
    }

    LOGF("Gas sensor present at 0x%02X attempting to initialize. Retry limit: %d", module_address,
         retryLimit);

    // The result of this determines if we are good to go
    moduleInitialized = attemptConnectionToSensor();

    if (moduleInitialized) {
        LOG(F("DFRobot Multi Gas Sensor acknowledged and initialized."));
        // Configure in passive mode with temperature compenstation off (default)
        moduleInitialized = configureSensorProperties();
    } else {
        ERROR(F("Failed to initialize DFRobot Multi Gas Sensor. Module disabled."));
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFMultiGasSensor::measure() {
    FUNCTION_START(this);
    if (!moduleInitialized) {
        return;
    }

    if (checkDeviceConnection()) {
        if (gasSensor.dataIsAvailable()) {
            LOG(F("Sensor has data availible. Reading ..."));
        } else {
            LOG(F("Sensor data not available yet; retaining the previous reading."));
            return;
        }

        // Read the concentration
        currentConcentration = gasSensor.readGasConcentrationPPM();

        // And Temperature
        currentTemperature = gasSensor.readTempC();
    } else {
        ERROR(F("No acknowledge received from DFRobot Multi Gas Sensor."));
        moduleInitialized = false;
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFMultiGasSensor::package() {
    FUNCTION_START(this);
    if (moduleInitialized) {
        JsonObject json = manInst->get_data_object(getModuleName());
        json[currentGasType] = currentConcentration;
        json["Temp(C)"] = currentTemperature;
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFMultiGasSensor::power_up() {
    FUNCTION_START(this);

    bool reconnected = false;
    if (powersDown || !moduleInitialized) {
        // If we are unable to connect to the sensor in power up this should disable the module for
        // atleast this run, decide if we want to do this or not
        moduleInitialized = attemptConnectionToSensor();
        reconnected = moduleInitialized;
    } else {
        // Module assumed to be initialized and to have called .begin() if never powered down
        // Prevents too many duplicate .begin() calls
        // moduleInitialized = checkDeviceConnection();
        moduleInitialized = checkDeviceConnection();
    }

    if (moduleInitialized) {
        // A power-cycled/reconnected board loses its acquisition settings.
        if (reconnected) {
            moduleInitialized = configureSensorProperties();
        }
    }
    if (moduleInitialized) {
        LOG(F("DFRobot Multi Gas sensor powered on successfully!"));
    } else {
        ERROR(F("DFRobot Multi Gas sensor failed to power on and has been disabled."));
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_DFMultiGasSensor::attemptConnectionToSensor() {
    FUNCTION_START(this);

    /* Attempt a set number of times to initialize the sensor */
    for (uint8_t retryCount = 0; retryCount < retryLimit; retryCount++) {
        LOOM_FEED_WATCHDOG();
        LOGF("Attempting to connect to sensor... Attempt %u / %u ", retryCount + 1, retryLimit);

        // If we do successfully begin the sensor we want to stop the loop immediatly and move on to
        // the next part of initialization
        if (gasSensor.begin()) {
            LOG(F("DFRobot Multi Gas Sensor connected! "));
            moduleInitialized = true;
            return true;
        }

        // If we have reached the max number of retries, then we want to just stop and disable the
        // module
        if (retryCount == retryLimit - 1) {
            ERRORF("Failed to connect to DFRobot Multi Gas Sensor after %u attempts. ", retryLimit);
            return false;
        }

        LOG(F("Waiting 3 seconds before attempting to retry..."));
        // Read delay
        unsigned long startMillis = millis();
        while ((millis() - startMillis) < 3000) {
            delay(1);
        }
    }

    // We shouldn't be able to make it here but if we do it was probably bad
    FUNCTION_END;
    return false;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_DFMultiGasSensor::configureSensorProperties(DFRobot_GAS::eMethod_t aquireMode,
                                                      DFRobot_GAS::eSwitch_t gasCompMode) {
    FUNCTION_START(this);
    // Set aquire mode to passive so we are able to request data from it whenever
    LOGF("Gas sensor %s (0x%02X): requesting %s acquisition mode over I2C",
         getModuleName(), module_address, aquireMode == gasSensor.PASSIVITY ? "PASSIVE" : "INITIATIVE");
    LOOM_FEED_WATCHDOG(); // Start this operation with the full guard; never feed a blocked call.
    bool modeAccepted;
    {
        FUNCTION_START(this, "Gas board: request acquisition mode and read acknowledgement (I2C)");
        modeAccepted = gasSensor.changeAcquireMode(aquireMode);
    }
    LOOM_FEED_WATCHDOG();
    if (!modeAccepted) {
        ERRORF("Gas sensor %s (0x%02X) did not acknowledge acquisition mode; disabled until a later recovery",
               getModuleName(), module_address);
        return false;
    }
    delay(1000);
    LOGF("Acquire Mode set to %hs", aquireMode == gasSensor.PASSIVITY ? "PASSIVE" : "INITIATIVE");

    // Set temperature compensation
    LOG(F("Setting temp compensation..."));
    {
        FUNCTION_START(this, "Gas board: set compensation and read temperature (I2C)");
        gasSensor.setTempCompensation(gasCompMode);
    }
    LOOM_FEED_WATCHDOG();
    delay(1000);
    LOGF("Temp compensation set to %hs", gasCompMode == gasSensor.OFF ? "OFF" : "ON");

    // Gas type is a board property, not a sample. Cache the allocation-free protocol result once
    // for this sensor instance.
    if (currentGasType[0] == '\0') {
        FUNCTION_START(this, "Gas board: identify gas type (I2C)");
        const char *gasType = gasSensor.queryGasTypeFixed();
        if (gasType == nullptr || gasType[0] == '\0') {
            strncpy(currentGasType, "INV_TYPE", sizeof(currentGasType));
        } else {
            strncpy(currentGasType, gasType, sizeof(currentGasType) - 1);
        }
        currentGasType[sizeof(currentGasType) - 1] = '\0';
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
