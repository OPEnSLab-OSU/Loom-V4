#include "Loom_AS7262.h";

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_AS7262::Loom_AS7262(
                        Manager& man,
                        bool useMux, 
                        int addr,
                        uint8_t gain,
                        uint8_t mode,
                        uint8_t integration_time 
                    ) : Module("AS7262"), manInst(&man), gain(gain), mode(mode), integration_time(integration_time) {
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
        printModuleName(); Serial.println("Failed to initialize AS7262! Check connections and try again...");
        moduleInitialized = false;
        return;
    }
    else{
        printModuleName(); Serial.println("Successfully initialized AS7262!");
        asInst.setGain(gain);
        asInst.setMeasurementMode(mode);
        asInst.setIntegrationTime(integration_time);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_AS7262::measure() {
    if(moduleInitialized){
        if(needsReinit){
            initialize();
        }
        else if(!checkDeviceConnection()){
            printModuleName(); Serial.println("No acknowledge received from the device");
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
        json["Color_1"] = color[0];
        json["Color_2"] = color[1];
        json["Color_3"] = color[2];
        json["Color_4"] = color[3];
        json["Color_5"] = color[4];
        json["Color_6"] = color[5];
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