#include "Loom_VCNL4030.h"
#include "Logger.h"

Loom_VCNL4030::Loom_VCNL4030(Manager &man, int address, bool useMux)
    : I2CDevice("VCNL4030"), managerInstance(&man) {
    module_address = address;
    if (!useMux) {
        managerInstance->registerModule(this);
    }
}

void Loom_VCNL4030::initialize() {
    FUNCTION_START;
    if (!vcnl.begin(module_address)) {
        ERROR(F("Failed to initialize VCNL4030! Check connections and try again..."));
        moduleInitialized = false;
    } else {
        LOG(F("Successfully initialized module VCNL4030!"));
        delay(250); 

    FUNCTION_END;
}

void Loom_VCNL4030::measure() {
    FUNCTION_START;
    if (moduleInitialized) {
        bool connectionStatus = checkDeviceConnection();

        if (connectionStatus && needsReinit) {
            initialize();
            needsReinit = false;
        } else if (!connectionStatus) {
            ERROR(F("No acknowledge received from VCNL4030 module"));
            FUNCTION_END;
            return;
        }

        proximity = vcnl.readProximity();
        lux = vcnl.readLux();
        white = vcnl.readWhite();
    }
    FUNCTION_END;
}

void Loom_VCNL4030::package() {
    FUNCTION_START;
    if (moduleInitialized) {
        JsonObject json = managerInstance->get_data_object(getModuleName());
        json["Proximity"] = proximity;
        json["Lux"] = lux;
        json["White"] = white;
    }
    FUNCTION_END;
}

// begin() re-applies the sensor's default config, so it also restores it after Hypnos cuts power
void Loom_VCNL4030::power_up() {
    FUNCTION_START;
    if (moduleInitialized) {
        if (!vcnl.begin(module_address)) {
            ERROR(F("VCNL4030 not responding after wake"));
        } else {
            delay(250);
        }
    }
    FUNCTION_END;
}
