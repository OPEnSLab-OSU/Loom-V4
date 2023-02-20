#include "Loom_MS5803.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MS5803::Loom_MS5803(Manager& man, byte address, bool useMux) : I2CDevice("MS5803"), manInst(&man), inst(address, 512) {
    module_address = address;

    if(!useMux)
        manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::initialize(){
    FUNCTION_START;
    // Setup is backwards apparently
    if(inst.initializeMS_5803(false)){
        printModuleName("Failed to initialize sensor!");
        moduleInitialized = false;
    }
    else{
        printModuleName("Successfully Initialized!");
        
        // Wait 3 seconds after initializing
        delay(3000);
    }
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::measure(){
    FUNCTION_START;
    // Make sure the sensor initialized correctly
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
    
        // Reinit the module every measure
        inst.initializeMS_5803(false);
        delay(1000);

        inst.readSensor();
        sensorData[0] = inst.temperature();
        sensorData[1] = inst.pressure();
    }
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::package(){
    FUNCTION_START;
    // Make sure the sensor initialized correctly
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Temperature"] = sensorData[0];
        json["Pressure"] = sensorData[1];
    }
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::power_up(){
    FUNCTION_START;
    initialize();
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////