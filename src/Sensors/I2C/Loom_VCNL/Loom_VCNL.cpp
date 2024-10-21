#include "Loom_VCNL.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_VCNL::Loom_VCNL(
                            Manager& man,
                            int address,
                            bool useMux
                    ) : I2CDevice("VCNL4010"), manInst(&man), tsl(address) {
                        module_address = address;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VCNL::initialize() {
    FUNCTION_START;
    if(!tsl.begin()){
        ERROR(F("Failed to initialize VCNL4010! Check connections and try again..."));
        moduleInitialized = false;
    }
    else{

        // Set the gain and integration time of the sensor
        //tsl.setGain(gain);
        //tsl.setTiming(intTime);

        LOG(F("Successfully initialized VCNL4010!"));
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VCNL::measure() {
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
        uint16_t ambientLight = vcnl.readAmbient();
        uint16_t proximity = vcnl.readProximity();



        // Make sure the value is actually valid
        // if(visible > 65533)
        //     lightLevels[0] = 0;
        // else
        //     lightLevels[0] = visible;
        
        // lightLevels[1] = tsl.getLuminosity(TSL2591_INFRARED);
        // lightLevels[2] = tsl.getLuminosity(TSL2591_FULLSPECTRUM);

        // If it is the first packet measure again to get accurate readings
        // if(manInst->get_packet_number() == 1){
        //     // Pull the data from the sensor
        //     lightLevels[0] = tsl.getLuminosity(TSL2591_VISIBLE);
        //     lightLevels[1] = tsl.getLuminosity(TSL2591_INFRARED);
        //     lightLevels[2] = tsl.getLuminosity(TSL2591_FULLSPECTRUM);
        // }
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VCNL::package() {
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Ambient Light"] = ambientLight;
        json["Proximity"] = proximity;
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
// void Loom_VCNL::power_up() {
//     FUNCTION_START;
//     if(moduleInitialized){
//         // Set the gain and integration time of the sensor
//         tsl.setGain(gain);
//         tsl.setTiming(intTime);
//     }
//     FUNCTION_END;
// }
//////////////////////////////////////////////////////////////////////////////////////////////////////
