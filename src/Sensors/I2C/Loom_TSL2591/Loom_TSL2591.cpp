#include "Loom_TSL2591.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_TSL2591::Loom_TSL2591(
                            Manager& man,
                            int address,
                            bool useMux,   
                            tsl2591Gain_t light_gain, 
                            tsl2591IntegrationTime_t integration_time
                    ) : I2CDevice("TSL2591"), manInst(&man), tsl(address), gain(light_gain), intTime(integration_time) {
                        module_address = address;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::initialize() {
    FUNCTION_START;
    if(!tsl.begin()){
        printModuleName("Failed to initialize TSL2591! Check connections and try again...");
        moduleInitialized = false;
    }
    else{

        // Set the gain and integration time of the sensor
        tsl.setGain(gain);
        tsl.setTiming(intTime);

        printModuleName("Successfully initialized TSL2591!");
    }
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::measure() {
    FUNCTION_START;
    if(moduleInitialized){
        // Get the current connection status
        bool connectionStatus = checkDeviceConnection();

        // If we are connected and we need to reinit
        if(connectionStatus && needsReinit){
            initialize();
            needsReinit = false;
        }

        // If we are not connected
        else if(!connectionStatus){
            printModuleName("No acknowledge received from the device");
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
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::package() {
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Visible"] = lightLevels[0];
        json["Infrared"] = lightLevels[1];
        json["Full_Spectrum"] = lightLevels[2];
    }
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::print_measurements() {

	Serial.println("Measurements:");
	Serial.println("\tVisible: " + String(lightLevels[0]));
	Serial.println("\tIR: " + String(lightLevels[1]));
	Serial.println("\tFull: " + String(lightLevels[2]));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_TSL2591::power_up() {
    FUNCTION_START;
    if(moduleInitialized){
        // Set the gain and integration time of the sensor
        tsl.setGain(gain);
        tsl.setTiming(intTime);
    }
    FUNCTION_END("void");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
