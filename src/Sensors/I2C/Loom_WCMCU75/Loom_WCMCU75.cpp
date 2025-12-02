#include "Loom_WCMCU75.h"
#include "Logger.h"

Loom_WCMCU75::Loom_WCMCU75(
                    Manager& man, 
                    int address, 
                    bool useMux
                ) : I2CDevice("WCMCU75"), manInst(&man), wcmcu75(address) { // no clue how this works
        module_address = address; // inherited from I2CDevice

        if (!useMux)
            manInst->registerModule(this); // inherited from manager
    }

void Loom_WCMCU75::initialize(){
    FUNCTION_START; // used for logging on serial monitor

    Wire.begin();
    moduleInitialized = true;

    FUNCTION_END;
}

void Loom_WCMCU75::measure(){
    FUNCTION_START;

    if(moduleInitialized){ // inherited from I2CDevice    
        // temperature = wcmcu75.temp();
        // tos = wcmcu75.tos();
        // thyst = wcmcu75.thyst();

        temperature = 100;
        tos = 150;
        thyst = 200;
    }

    FUNCTION_END;
}

void Loom_WCMCU75::package(){
    FUNCTION_START;

    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Temp_C"] = temperature;
        json["Tos_C"] = tos;
        json["Thyst_C"] = thyst;
    }

    FUNCTION_END;
}

void Loom_WCMCU75::power_up(){
    FUNCTION_START;

    wcmcu75.shutdown(false);

    FUNCTION_END;
}

void Loom_WCMCU75::power_down(){
    FUNCTION_START;

    wcmcu75.shutdown(true);
    moduleInitialized = false;

    FUNCTION_END;

}