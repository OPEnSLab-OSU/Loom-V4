#include "DUST.h"

Loom_NOVASDS011::Loom_NOVASDS011(Manager& man) : Module("NOVASDS011"), manInst(&man) {

    manInst->registerModule(this);
}

void Loom_NOVASDS011::initialize(){

    nova.begin();
}

void Loom_NOVASDS011::measure(){
    float temp25, temp10;
    int err = nova.read(temp25, temp10);

    // If there was no error we want to update the readings
    if(!err){
        pm25 = temp25;
        pm10 = temp10;
    }else{
        printModuleName("Failed to read from sensor");
    }
}

void Loom_NOVASDS011::package(){
    JsonObject json = manInst-> get_data_object(getModuleName());
    json["PM2.5"] = pm25;
    json["PM_10"] = pm10;
}