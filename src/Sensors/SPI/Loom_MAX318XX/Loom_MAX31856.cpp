#include "Loom_MAX31856.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MAX31856::Loom_MAX31856(Manager& man, int samples, int chip_select) : Module("MAX31856"), manInst(&man), maxthermo(10), num_samples(samples) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::initialize(){
    if (!maxthermo.begin()) {
        printModuleName("Could not initialize thermocouple.");
        moduleInitialized = false;
        return;
    }
    else{
        printModuleName("Successfully initialized thermocouple.");
    }
    maxthermo.setThermocoupleType(MAX31856_TCTYPE_K);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::measure(){
    if(moduleInitialized){
        float temp = 0;

        // Collect the data however many times as specified
        for(int i = 0; i < num_samples; i++){
            temp += maxthermo.readThermocoupleTemperature();

            // Check and print any faults
            uint8_t fault = maxthermo.readFault();
            if (fault) {
                if (fault & MAX31856_FAULT_CJRANGE) printModuleName("Cold Junction Range Fault");
                if (fault & MAX31856_FAULT_TCRANGE) printModuleName("Thermocouple Range Fault");
                if (fault & MAX31856_FAULT_CJHIGH)  printModuleName("Cold Junction High Fault");
                if (fault & MAX31856_FAULT_CJLOW)   printModuleName("Cold Junction Low Fault");
                if (fault & MAX31856_FAULT_TCHIGH)  printModuleName("Thermocouple High Fault");
                if (fault & MAX31856_FAULT_TCLOW)   printModuleName("Thermocouple Low Fault");
                if (fault & MAX31856_FAULT_OVUV)    printModuleName("Over/Under Voltage Fault");
                if (fault & MAX31856_FAULT_OPEN)    printModuleName("Thermocouple Open Fault");
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
        json["Temperature"] = temperature;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////