#include "Loom_MS5803.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MS5803::Loom_MS5803(Manager& man, byte address) : Module("MS5803"), manInst(&man), inst(address, 512) {
    module_address = address;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::initialize(){
    printModuleName(); Serial.println("Initializing sensor...");
    Wire.begin();
    delay(50);

    // Setup is backwards apparently
    if(inst.initializeMS_5803()){
        printModuleName(); Serial.println("Failed to initialize sensor!");
        moduleInitialized = false;
    }
    else{
        printModuleName(); Serial.println("Successfully Initialized!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MS5803::measure(){
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