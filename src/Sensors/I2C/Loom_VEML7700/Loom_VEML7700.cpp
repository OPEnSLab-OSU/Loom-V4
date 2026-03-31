#include "Loom_VEML7700.h"
#include "Logger.h"

Loom_VEML7700::Loom_VEML7700(Manager &man, int address, bool useMux, 
    uint8_t gain, uint8_t int_time, uint8_t persistence)
    : I2CDevice("VEML7700"), managerInstance(&man), veml(), gain(gain), int_time(int_time), persistence(persistence){
        module_address = address;

        if(!useMux){
            managerInstance->registerModule(this);
        }
    }

void Loom_VEML7700::initialize(){
    FUNCTION_START;
    if(!veml.begin()){
        ERROR("VEML7700 failed to initialize! Check Connection and try again...");
        moduleInitialized = false;
    }
    else{
        LOG("VEML7700 successfully initialized!");
    }
    FUNCTION_END;
}

void Loom_VEML7700::measure(){
    FUNCTION_START;
    if (moduleInitialized){
        // Connection status
        bool connectionStatus = checkDeviceConnection();
        
        // Connected and needs to reinitialize
        if(connectionStatus && needsReinit){
            initialize();
            needsReinit = false;
        }

        // No connection, end function
        else if(!connectionStatus){
            ERROR("No connection from VEML7700");
            FUNCTION_END;
            return;
        }
        // Read lux with normal method
        LOG("Measure VEML7700");
        Lux = veml.readLux();
    }
    else{
        ERROR("NOT INITIALIZED IN MEASURE!");
    }
    FUNCTION_END;
}

void Loom_VEML7700::package(){
    FUNCTION_START;
    if(moduleInitialized){
        // Create json object with this module's name
        JsonObject json = managerInstance->get_data_object(getModuleName());
        json["Lux"] = Lux;
    }
    Serial.println(Lux);
    FUNCTION_END;
}

void Loom_VEML7700::power_up(){
    FUNCTION_START;
    if(moduleInitialized){
        // Mimics the Adafruit veml7700 begin() function calls
        veml.enable(false);
        veml.setGain(gain);
        veml.setIntegrationTime(int_time);
        veml.setPersistence(persistence);
        veml.powerSaveEnable(false);
        veml.enable(true);
    }
    FUNCTION_END;
}

