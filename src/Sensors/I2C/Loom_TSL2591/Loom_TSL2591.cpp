#include "Loom_TSL2591.h";

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_TSL2591::Loom_TSL2591(
                            Manager& man,
                            int address,
                            bool useMux,   
                            tsl2591Gain_t light_gain, 
                            tsl2591IntegrationTime_t integration_time
                    ) : Module("TSL2591"), manInst(&man), tsl(address), gain(light_gain), intTime(integration_time) {
                        module_address = address;

                        // Register the module with the manager
                        if(!useMux)
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
    if(!checkDeviceConnection()){
        printModuleName(); Serial.println("No acknowledge received from the device");
        return;
    }

    // Pull the data from the sensor
    uint16_t visible = tsl.getLuminosity(TSL2591_VISIBLE);

    // Make sure the value is actually valid
    if(visible > 65533)
        lightLevels[0] = 0;
    else
        lightLevels[0] = visible;
    
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
    json["Full_Spectrum"] = lightLevels[2];
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