#include "Loom_AS7262.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_AS7262::Loom_AS7262(
                        Manager& man,
                        bool useMux, 
                        int addr,
                        uint8_t gain,
                        uint8_t mode,
                        uint8_t integration_time 
                    ) : I2CDevice("AS7262"), manInst(&man), gain(gain), mode(mode), integration_time(integration_time) {
                        module_address = addr;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7262::initialize() {

    // If we have less than 2 bytes of json from the sensor
    if(!asInst.begin()){
        printModuleName("Failed to initialize AS7262! Check connections and try again...");
        moduleInitialized = false;
        return;
    }
    else{
        printModuleName("Successfully initialized AS7262!");
        asInst.setGain(gain);
        asInst.setMeasurementMode(mode);
        asInst.setIntegrationTime(integration_time);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7262::measure() {
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

        // Color
        color[0] = asInst.getViolet();
        color[1] = asInst.getBlue();
        color[2] = asInst.getGreen();
        color[3] = asInst.getYellow();
        color[4] = asInst.getOrange();
        color[5] = asInst.getRed();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7262::package() {
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["C_1"] = color[0];
        json["C_2"] = color[1];
        json["C_3"] = color[2];
        json["C_4"] = color[3];
        json["C_5"] = color[4];
        json["C_6"] = color[5];
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7262::power_up() {
    if(moduleInitialized){
        asInst.setGain(gain);
        asInst.setMeasurementMode(mode);
        asInst.setIntegrationTime(integration_time);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////