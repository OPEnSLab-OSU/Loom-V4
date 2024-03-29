#include "Loom_ADS1232.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ADS1232::Loom_ADS1232(Manager& man, int num_samples, long offset, float scale) : Module("ADS1232"), manInst(&man), inst(ADS1232_Lib(A2, A1, A0)) {
    // Set offset, scale, and number of samples
    this->offset = offset;
    this->scale = scale;
    this->num_samples = num_samples;
    // Set pins
    inst.PDWN = A2;
    inst.SCLK = A1;
    inst.DOUT = A0;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1232::initialize(){
    calibrate();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1232::power_up(){
    // Turn on pins for SPI communication
    inst.power_up();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1232::power_down(){
    // Disable pins to save power
    inst.power_down();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
        
//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1232::measure(){
    if (!inst.is_ready()) inst.power_up();
    weight = inst.units_read(num_samples);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1232::package(){
    JsonObject json = manInst->get_data_object(getModuleName());
    json["weight"] = weight;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1232::calibrate(){
    inst.OFFSET = this->offset;
    inst.SCALE = this->scale;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////