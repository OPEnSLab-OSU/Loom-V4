#include "Loom_MAX31856.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MAX31856::Loom_MAX31856(Manager& man, int samples, int chip_select, int mosi, int miso, int sclk, bool farenheit) : Module("MAX31856"), manInst(&man), num_samples(samples), farenheit_display(farenheit) {
    manInst->registerModule(this);
    if(mosi != -1 && miso != -1 && sclk != -1){
        maxthermo = new Adafruit_MAX31856(chip_select, mosi, miso, sclk);
    }
    else{
        maxthermo = new Adafruit_MAX31856(chip_select);
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::initialize(){
    SPI.setDataMode(SPI_MODE1);
    if (!maxthermo->begin()) {
        SPI.setDataMode(SPI_MODE0);
        ERROR(F("Could not initialize thermocouple."));
        moduleInitialized = false;
        return;
    }
    else{
        LOG(F("Successfully initialized thermocouple."));
    }
    SPI.setDataMode(SPI_MODE1);
    maxthermo->setThermocoupleType(MAX31856_TCTYPE_K);
    SPI.setDataMode(SPI_MODE0);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::measure(){
    if(moduleInitialized){
        float temp = 0;

       
        // Collect the data however many times as specified
        for(int i = 0; i < num_samples; i++){
            SPI.setDataMode(SPI_MODE1);
            temp += maxthermo->readThermocoupleTemperature();

            // Check and print any faults
            uint8_t fault = maxthermo->readFault();
            SPI.setDataMode(SPI_MODE0);
            if (fault) {
                if (fault & MAX31856_FAULT_CJRANGE) ERROR(F("Cold Junction Range Fault"));
                if (fault & MAX31856_FAULT_TCRANGE) ERROR(F("Thermocouple Range Fault"));
                if (fault & MAX31856_FAULT_CJHIGH)  ERROR(F("Cold Junction High Fault"));
                if (fault & MAX31856_FAULT_CJLOW)   ERROR(F("Cold Junction Low Fault"));
                if (fault & MAX31856_FAULT_TCHIGH)  ERROR(F("Thermocouple High Fault"));
                if (fault & MAX31856_FAULT_TCLOW)   ERROR(F("Thermocouple Low Fault"));
                if (fault & MAX31856_FAULT_OVUV)    ERROR(F("Over/Under Voltage Fault"));
                if (fault & MAX31856_FAULT_OPEN)    ERROR(F("Thermocouple Open Fault"));
                break;
            }
        }
        
        
        // Get the average temperature
        temperature = temp / num_samples;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::package(){
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["TemperatureC"] = temperature;
        if (fer_display){
            json["TemperatureF"] = (temperature*9/5)+32;
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////