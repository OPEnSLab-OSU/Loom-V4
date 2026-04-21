#include "Loom_VCNL4020.h"
#include "Logger.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// Constructor initialize config settings
Loom_VCNL4020::Loom_VCNL4020(Manager &man, int address, bool useMux, vcnl4020_ambientrate ambRate,
                                vcnl4020_averaging ambAvg, uint8_t proxLED, vcnl4020_proxrate proxRate, vcnl4020_proxfreq proxFreq)
                            : I2CDevice("VCNL4020"), 
                            managerInstance(&man), 
                            vcnl(),  
                            ambRate(ambRate), 
                            ambAvg(ambAvg), 
                            proxLED(proxLED), 
                            proxRate(proxRate), 
                            proxFreq(proxFreq) 
    {
        module_address = address;

        if(!useMux){
            managerInstance->registerModule(this);
        }
    }
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void Loom_VCNL4020::initialize(){
    FUNCTION_START;
    if(!vcnl.begin()) {
        ERROR(F("Failed to initialize VCNL4020! Check connections and try again..."));
        moduleInitialized = false;
    } else {
        LOG(F("Successfully initialized module VCNL4020!"));
        delay(250); // Give sensor time to produce a reading
    }
    FUNCTION_END;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VCNL4020::measure(){
    FUNCTION_START;
    Serial.println("VCNL4020 Measure\n");
    if(moduleInitialized) {
        bool connectionStatus = checkDeviceConnection();

        // If need to reinitialize
        if(connectionStatus && needsReinit){
            initialize();
            needsReinit = false;
        }

        // If no connection
        else if(!connectionStatus){
            ERROR("No acknoledge recieved from VCNL4020 module");
            FUNCTION_END;
            return;
        }

        // When everything works, measure
        LOG("Running Adafruit VCNL4020 measure functions");
    
        // make sure ALS works
        uint32_t start = millis();
        while (!vcnl.isAmbientReady()) {
            if (millis() - start > 100) {
                ERROR("Ambient timeout");
                return;
            }
        }

        // Measure ambient light and proximity
        ambientLight = vcnl.readAmbient();
        proximity = vcnl.readProximity();

    }
    FUNCTION_END;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VCNL4020::package(){
    FUNCTION_START;
    if(moduleInitialized) {
        JsonObject json = managerInstance->get_data_object(getModuleName());
        // higher bit values indicate more light
        json["Ambient Light_counts"] = ambientLight;
        json["Proximity"] = proximity;
    }
    Serial.println(ambientLight);
    Serial.println(proximity);
    FUNCTION_END;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_VCNL4020::power_up(){
    FUNCTION_START;
    if(moduleInitialized){
        // If sensor does not reconnect upon wakeup
        if (!checkDeviceConnection()) {
            ERROR("VCNL4020 not responding after wake");
            FUNCTION_END;
            return;
        }

        // Mimic adafruit VCNL4020 begin() function calls to reconfigure settings upon power on
        vcnl.enable(false, false, false);

        vcnl.setOnDemand(false, false);
        vcnl.setProxRate(proxRate);
        vcnl.setProxLEDmA(proxLED);
        vcnl.setAmbientRate(ambRate);
        vcnl.setAmbientAveraging(ambAvg);
        vcnl.setProxFrequency(proxFreq);

        vcnl.setInterruptConfig(true, true, false, false, INT_COUNT_1);
        
        vcnl.enable(true, true, true);

        delay(250); // sensor needs time to produce a reading
    }
    FUNCTION_END;
}
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
