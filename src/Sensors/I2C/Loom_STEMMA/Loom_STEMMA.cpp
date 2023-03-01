#include "Loom_STEMMA.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_STEMMA::Loom_STEMMA(
                        Manager& man, 
                        int addr,
                        bool useMux  
                    ) : I2CDevice("STEMMA"), manInst(&man), address(addr) {
                        module_address = addr;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::initialize() {
    FUNCTION_START;
    char output[100];
    if(!stemma.begin(address)){
        LOG("Failed to initialize STEMMA! Check connections and try again...");
        moduleInitialized = false;
    }
    else{
        snprintf(output, 100, "Successfully initialized STEMMA Version: %u", stemma.getVersion());
        LOG(output);
        
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::measure() {
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
            LOG("No acknowledge received from the device");
            return;
        }
        

        // Pull the data from the sensor
        temperature = stemma.getTemp();
        cap = stemma.touchRead(0);
    }
    FUNCTION_END;
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::package() {
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Temperature"] = temperature;
        json["Capacitive"] = cap;
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////