#include "Loom_STEMMA.h"

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
    if(!stemma.begin(address)){
        printModuleName("Failed to initialize STEMMA! Check connections and try again...");
        moduleInitialized = false;
    }
    else{
        printModuleName("Successfully initialized STEMMA Version: " + String(stemma.getVersion()));
        
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::measure() {
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
            printModuleName("No acknowledge received from the device");
            return;
        }
        

        // Pull the data from the sensor
        temperature = stemma.getTemp();
        cap = stemma.touchRead(0);
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_STEMMA::package() {
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Temperature"] = temperature;
        json["Capacitive"] = cap;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////