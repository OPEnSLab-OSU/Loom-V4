#include "Loom_VCNL4020.h"
#include "Logger.h"

Loom_VCNL4020::Loom_VCNL4020(Manager &man, int address, bool useMux, uint16_t ambRate,
                                uint16_t ambAvg, uint16_t proxLED, uint16_t proxRate)
    : I2CDevice("VCNL4020"), managerInstance(&man), vcnl(),  ambRate(ambRate), ambAvg(ambAvg), proxLED(proxLED), proxRate(proxRate), proxFreq(proxFreq) {
        module_address = address;

        if(!useMux){
            managerInstance->registerModule(this);
        }
    }

void Loom_VCNL4020::initialize(){
    FUNCTION_START;
    if(!vcnl.begin()) {
        ERROR(F("Failed to initialize VCNL4020! Check connections and try again..."));
        moduleInitialized = false;
    } else {
        LOG(F("Successfully initialized module VCNL4020!"));
    }
    FUNCTION_END;
}

void Loom_VCNL4020::measure(){
    FUNCTION_START;
    printf("VCNL4020 Measure\n");
    if(moduleInitialized) {
        bool connectionStatus = checkDeviceConnection();

        if(connectionStatus && needsRenit){
            initialize();
            needsReinit = false;
        }

        else if(!connectionStatus){
            ERROR("No acknoledge recieved from VCNL4020 module")
            FUNCTION_END;
            return;
        }
        LOG("Running Adafruit VCNL4020 measure functions");
        ambientLight = vcnl.readAmbient();
        proximity = vcnl.readProximity();

    }
    FUNCTION_END;
}

void Loom_VCNL4020::package(){
    FUNCTION_START;
    if(moduleInitialized) {
        JsonObject json = manInst->get_data_object(getModuleName());
        // higher bit values indicate more light
        json["Ambient Light_counts"] = ambientLight;
        json["Proximity"] = proximity;
    }
    Serial.println(ambientLight);
    Serial.println(proximity);
    FUNCTION_END;
}

void Loom_VCNL4020::power_up(){
    FUNCTION_START;
    if(moduleInitialized){
        
        if (!checkDeviceConnection()) {
            ERROR("VCNL4020 not responding after wake");
            FUNCTION_END;
        }

        // Mimic adafruit VCNL4020 begin() function calls to reconfigure settings upon power on
        vcnl.enable(false, false, false);

        vcnl.setOnDemand(false);
        vcnl.setProxRate(proxRate);
        vcnl.setProxLEDmA(proxLED);
        vcnl.setAmbientRate(ambRate);
        vcnl.setAmbientAveraging(ambAvg);
        vcnl.setProxFrequency(proxFreq);

        vcnl.setInterruptConfig(true, true, false, false, INT_COUNT_1);
        
        vcnl.enable(true, true, true);

        delay(10);
    }
    FUNCTION_END;
}
                               



                                