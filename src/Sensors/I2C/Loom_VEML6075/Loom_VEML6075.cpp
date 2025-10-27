#include "Loom_VEML6075.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_VEML6075::Loom_VEML6075(
                            Manager& man,
                            int address,
                            bool useMux
                    ) : I2CDevice("VEML6075"), manInst(&man), veml() {
                        module_address = address;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VEML6075::initialize() {
    FUNCTION_START;
    if(!veml.begin()){
        ERROR(F("Failed to initialize VEML6075! Check connections and try again..."));
        moduleInitialized = false;
    }
    else{
        LOG(F("Successfully initialized VEML6075!"));
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VEML6075::measure() {
    FUNCTION_START;
    printf("VEML6075 Measure\n");
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
            ERROR(F("No acknowledge received from the device"));
            FUNCTION_END;
            return;
        }
        LOG(F("Measure VEML6075"));
        // Pull the data from the sensor
        UVA = veml.readUVA();
        UVB = veml.readUVB();
        UVI = veml.readUVI();
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VEML6075::package() {
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        //Unitless, higher values indicate more light
        json["UltravioletA_counts"] = UVA;
        json["UltravioletB_counts"] = UVB;
        json["UltravioletIndex"] = UVI;
    } 
    Serial.println(UVA);
    Serial.println(UVB);
    Serial.println(UVI);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

