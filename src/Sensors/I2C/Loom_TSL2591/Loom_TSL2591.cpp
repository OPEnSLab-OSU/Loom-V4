#include "Loom_TSL2591.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_TSL2591::Loom_TSL2591(
                            Manager& man,
                            int address,
                            bool useMux,
                            tsl2591Gain_t light_gain,
                            tsl2591IntegrationTime_t integration_time
                    ) : I2CDevice("TSL2591"), manInst(&man), tsl(address), gain(light_gain), intTime(integration_time) {
                        module_address = address;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::initialize() {
    FUNCTION_START;
    if(!tsl.begin()){
        ERROR(F("Failed to initialize TSL2591! Check connections and try again..."));
        moduleInitialized = false;
    }
    else{

        // Set the gain and integration time of the sensor
        tsl.setGain(gain);
        tsl.setTiming(intTime);

        LOG(F("Successfully initialized TSL2591!"));
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::measure() {
    FUNCTION_START;
    if(moduleInitialized){
        // Get the current connection status
        bool connectionStatus = checkDeviceConnection();

        // If we are connected and we need to reinit
        if(connectionStatus && needsReinit){
            initialize();
            needsReinit = false;
        }

        // If we are not connected
        else if(!connectionStatus){
            ERROR(F("No acknowledge received from the device"));
            FUNCTION_END;
            return;
        }

        // Pull the data from the sensor
        uint16_t visible = tsl.getLuminosity(TSL2591_VISIBLE);

        // Make sure the value is actually valid
        if(visible > 65533)
            lightLevels[0] = 0;
        else
            lightLevels[0] = visible;

        lightLevels[1] = tsl.getLuminosity(TSL2591_INFRARED);
        lightLevels[2] = tsl.getLuminosity(TSL2591_FULLSPECTRUM);

        // If it is the first packet measure again to get accurate readings
        if(manInst->get_packet_number() == 1){
            // Pull the data from the sensor
            lightLevels[0] = tsl.getLuminosity(TSL2591_VISIBLE);
            lightLevels[1] = tsl.getLuminosity(TSL2591_INFRARED);
            lightLevels[2] = tsl.getLuminosity(TSL2591_FULLSPECTRUM);
        }
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::package() {
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Visible"] = lightLevels[0];
        json["Infrared"] = lightLevels[1];
        json["Full_Spectrum"] = lightLevels[2];
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::power_up() {
    FUNCTION_START;
    if(moduleInitialized){
        // Set the gain and integration time of the sensor
        tsl.setGain(gain);
        tsl.setTiming(intTime);
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_TSL2591::autoLux() {
    FUNCTION_START;
    const tsl2591Gain_t gains[] = {TSL2591_GAIN_LOW, TSL2591_GAIN_MED,
                                    TSL2591_GAIN_HIGH, TSL2591_GAIN_MAX};

    const tsl2591IntegrationTime_t intTimes[] = {TSL2591_INTEGRATIONTIME_100MS, TSL2591_INTEGRATIONTIME_200MS,
                                                TSL2591_INTEGRATIONTIME_300MS, TSL2591_INTEGRATIONTIME_400MS,
                                                TSL2591_INTEGRATIONTIME_500MS, TSL2591_INTEGRATIONTIME_600MS};

    uint8_t gainIdx = 0;
    uint8_t itIdx = 0;

    // Start at appropriate gain/ integration time
    tsl.setGain(gains[gainIdx]);
    tsl.setTiming(intTimes[itIdx]);

    // Read in the luminosity value
    uint32_t raw = tsl.getFullLuminosity();
    // Raw value full spectrum
    int rawFull = raw & 0xFFFF;
    // IR light
    int ir = raw >> 16;
    // Visible light
    int visible = rawFull - ir;

    if(rawFull <= 100){
        while ((rawFull <= 100) && !((gainIdx == 3) && (itIdx == 5))) {
            if (gainIndex < 3) {
                tsl.setGain(gains[++gainIdx]);
            }
            else if (itIndex < 5) {
                tsl.setTiming(intTimes[++itIdx]);
            }
            raw = tsl.getFullLuminosity();
            rawFull = raw & 0xFFFF;
            ir = raw >> 16;
            visible = rawFull - ir;
        }
    }
    else {
        while ((rawFull > 10000) && (itIdx > 0)) {
            setIntegrationTime(intTimes[--itIdx]);
            raw = tsl.getFullLuminosity();
            rawFull = raw & 0xFFFF;
            ir = raw >> 16;
            visible = rawFull - ir;
        }
    }
    FUNCTION_END;
    return tsl.calculateLux(rawFull, ir);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
