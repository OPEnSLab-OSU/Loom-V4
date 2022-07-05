#include "../../../Loom_MAX31856.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MAX31856::Loom_MAX31856(Manager& man, int chip_select, int samples) : Module("MAX31856"), manInst(&man), max(chip_select), num_samples(samples) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::initialize(){
    max.begin();
    max.setThermocoupleType(MAX31856_TCTYPE_K);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::measure(){
    float temp = 0;

    // Collect the data however many times as specified
    for(int i = 0; i < num_samples; i++){
        temp += max.readThermocoupleTemperature();

        // Check and print any faults
		uint8_t fault = max.readFault();
		if (fault) {
			if (fault & MAX31856_FAULT_CJRANGE) printModuleName(); Serial.println("Cold Junction Range Fault");
			if (fault & MAX31856_FAULT_TCRANGE) printModuleName(); Serial.println("Thermocouple Range Fault");
			if (fault & MAX31856_FAULT_CJHIGH)  printModuleName(); Serial.println("Cold Junction High Fault");
			if (fault & MAX31856_FAULT_CJLOW)   printModuleName(); Serial.println("Cold Junction Low Fault");
			if (fault & MAX31856_FAULT_TCHIGH)  printModuleName(); Serial.println("Thermocouple High Fault");
			if (fault & MAX31856_FAULT_TCLOW)   printModuleName(); Serial.println("Thermocouple Low Fault");
			if (fault & MAX31856_FAULT_OVUV)    printModuleName(); Serial.println("Over/Under Voltage Fault");
			if (fault & MAX31856_FAULT_OPEN)    printModuleName(); Serial.println("Thermocouple Open Fault");
			break;
		}
    }
    
    // Get the average temperature
    temperature = temp / num_samples;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MAX31856::package(){
    JsonObject json = manInst->get_data_object(getModuleName());
    json["Temperature"] = temperature;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////