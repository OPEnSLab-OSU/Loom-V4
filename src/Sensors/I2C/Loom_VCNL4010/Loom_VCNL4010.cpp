#include "Loom_VCNL4010.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_VCNL4010::Loom_VCNL4010(
                            Manager& man,
                            int address,
                            bool useMux
                    ) : I2CDevice("VCNL4010"), manInst(&man), vcnl() {
                        module_address = address;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VCNL4010::initialize() {
    FUNCTION_START;
    if(!vcnl.begin()){
        ERROR(F("Failed to initialize VCNL4010! Check connections and try again..."));
        moduleInitialized = false;
    }
    else{
        LOG(F("Successfully initialized VCNL4010!"));
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VCNL4010::measure() {
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
        uint16_t ambientLight = vcnl.getAmbientLight();
        uint16_t proximity = vcnl.getProximity();
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VCNL4010::package() {
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Ambient Light"] = ambientLight;
        json["Proximity"] = proximity;
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

