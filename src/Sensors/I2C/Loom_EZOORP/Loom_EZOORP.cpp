#include "Loom_EZOORP.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_EZOORP::Loom_EZOORP(Manager& man, byte address, bool useMux) : EZOSensor("EZO-ORP"), manInst(&man){
    i2c_address = address;
    module_address = i2c_address;

    if(!useMux)
        manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZOORP::initialize(){
    Wire.begin();
    moduleInitialized = calibrate();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZOORP::measure(){
    if(moduleInitialized){

        // Attempt to read data from the sensor
        if(!readSensor()){
            ERROR(F("Failed to read sensor!"));
            return;
        }

        // Parse the constructed string
        orp = atof(getSensorData());
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZOORP::package(){
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["ORP"] = orp;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZOORP::power_down() {
    if(moduleInitialized){
        if(!sendTransmission("sleep")){
            ERROR(F("Failed to send 'sleep' command to device"));
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////