#include "Loom_MS5803.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MS5803::Loom_MS5803(Manager& man, bool useMux, byte address) : I2CSensor("MS5803"), manInst(&man), inst(address, 512) {
    module_address = address;

    if(!useMux)
        manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::initialize(){
    // Setup is backwards apparently
    if(inst.initializeMS_5803(false)){
        printModuleName(); Serial.println("Failed to initialize sensor!");
        moduleInitialized = false;
    }
    else{
        printModuleName(); Serial.println("Successfully Initialized!");
        
        // Wait 3 seconds after initializing
        delay(3000);
    }

    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::measure(){
    if(checkDeviceConnection()){
        printModuleName(); Serial.println("No acknowledge received from the device");
        return;
    }

    // Make sure the sensor initialized correctly
    if(moduleInitialized){
        inst.readSensor();
        sensorData[0] = inst.temperature();
        sensorData[1] = inst.pressure();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::package(){
    // Make sure the sensor initialized correctly
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Temperature"] = sensorData[0];
        json["Pressure"] = sensorData[1];
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////