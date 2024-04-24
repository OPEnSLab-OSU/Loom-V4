#include "Loom_VEML7700.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_VEML7700::Loom_VEML7700(
                            Manager& man,
                            int address,
                            bool useMux

                    ) : I2CDevice("VEML7700"), manInst(&man), veml(){
                        module_address = address;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VEML7700::initialize() { 
    FUNCTION_START;
    if(!veml.begin()){
        ERROR(F("Failed to initialize VEML7700! Check connections and try again..."));
        moduleInitialized = false;
    }
    else{
        LOG(F("Successfully initialized VEML7700!"));
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VEML7700::measure() {
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

    autoLux = veml.readLux(VEML_LUX_AUTO);
    rawALS = veml.readALS();
    rawWhite = veml.readWhite();
        
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VEML7700::package() {
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["rawALS"] = rawALS;
        json["rawWhite"] = rawWhite;
        json["autoLux"] = autoLux;
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////


