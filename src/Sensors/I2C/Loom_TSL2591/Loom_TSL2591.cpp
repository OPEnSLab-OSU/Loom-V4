#include "Loom_TSL2591.h";

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_TSL2591::Loom_TSL2591(
                            Manager& man, 
                            int address,  
                            tsl2591Gain_t light_gain, 
                            tsl2591IntegrationTime_t integration_time
                    ) : Module("TSL2591"), manInst(&man), tsl(address), gain(light_gain), intTime(integration_time) {

                        // Register the module with the manager
                        manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::initialize() {
    if(!tsl.begin()){
        printModuleName(); Serial.println("Failed to initialize TSL2591! Check connections and try again...");
    }
    else{

        // Set the gain and integration time of the sensor
        tsl.setGain(gain);
        tsl.setTiming(intTime);

        printModuleName(); Serial.println("Successfully initialized TSL2591!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::measure() {
    // Pull the data from the sensor
    lightLevels[0] = tsl.getLuminosity(TSL2591_VISIBLE);
    lightLevels[1] = tsl.getLuminosity(TSL2591_INFRARED);
    lightLevels[2] = tsl.getLuminosity(TSL2591_FULLSPECTRUM);

    // If it is the first packet measure again to get accurate readings
    if(manInst->get_packet_number() == 1){
        // Pull the data from the sensor
        lightLevels[0] = tsl.getLuminosity(TSL2591_VISIBLE);
        lightLevels[1] = tsl.getLuminosity(TSL2591_INFRARED);
        lightLevels[2] = tsl.getLuminosity(TSL2591_FULLSPECTRUM);
    }
    
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::package() {
    JsonObject json = manInst->get_data_object(getModuleName());
    json["Visible"] = lightLevels[0];
    json["Infrared"] = lightLevels[1];
    json["Full Spectrum"] = lightLevels[2];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::print_measurements() {

    printModuleName();
	Serial.println("Measurements:");
	Serial.println("\tVisible: " + String(lightLevels[0]));
	Serial.println("\tIR: " + String(lightLevels[1]));
	Serial.println("\tFull: " + String(lightLevels[2]));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////