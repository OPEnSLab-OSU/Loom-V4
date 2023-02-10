#include "Loom_AS7263.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_AS7263::Loom_AS7263(
                        Manager& man,
                        bool useMux,
                        int addr,
                        uint8_t gain,
                        uint8_t mode,
                        uint8_t integration_time 
                    ) : I2CDevice("AS7263"), manInst(&man), gain(gain), mode(mode), integration_time(integration_time) {
                        module_address = addr;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                        
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7263::initialize() {

    // If we have less than 2 bytes of json from the sensor
    if(!asInst.begin()){
        printModuleName("Failed to initialize AS7263! Check connections and try again...");
        moduleInitialized = false;
        return;
    }
    else{
        printModuleName("Successfully initialized AS7263!");
        asInst.setGain(gain);
		asInst.setMeasurementMode(mode);
        asInst.setIntegrationTime(integration_time);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7263::measure() {
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
    
        // Take a measurement and wait for it to be ready
        asInst.takeMeasurements();
        while(!asInst.dataAvailable()){
            delay(5);
        }

        // NIR
        nir[0] = asInst.getCalibratedR();
        nir[1] = asInst.getCalibratedS();
        nir[2] = asInst.getCalibratedT();
        nir[3] = asInst.getCalibratedU();
        nir[4] = asInst.getCalibratedV();
        nir[5] = asInst.getCalibratedW();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7263::package() {
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["NIR_1"] = nir[0];
        json["NIR_2"] = nir[1];
        json["NIR_3"] = nir[2];
        json["NIR_4"] = nir[3];
        json["NIR_5"] = nir[4];
        json["NIR_6"] = nir[5];
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7263::power_up() {
    if(moduleInitialized){
        asInst.setGain(gain);
		asInst.setMeasurementMode(mode);
        asInst.setIntegrationTime(integration_time);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////