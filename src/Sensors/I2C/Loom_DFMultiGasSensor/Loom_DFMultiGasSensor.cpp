#include "Loom_DFMultiGasSensor.h"
#include "Wire.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_DFMultiGasSensor::Loom_DFMultiGasSensor(
                            Manager& man,
                            uint8_t address,
                            uint8_t initializationRetyLimit,
                            bool useMux
                    ) : I2CDevice("DFR_MultiGasSensor"), 
                        manInst(&man), 
                        retryLimit(initializationRetyLimit),
                        gasSensor(&Wire, address) 
                    {
                        module_address = address;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFMultiGasSensor::initialize() {
    FUNCTION_START;

    LOG(F("Begin DFRobot Multi Gas Sensor Initialization..."));

    // The result of this determines if we are good to go
    moduleInitialized = attemptConnectionToSensor();

    if(moduleInitialized){
        // Configure in passive mode with temperature compenstation off (default)
        configureSensorProperties();
    }
    else{
        ERROR(F("Failed to initialize DFRobot Multi Gas Sensor. Module disabled."));
    }
        

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFMultiGasSensor::measure() {
    FUNCTION_START;
    if(moduleInitialized ){
        if(checkDeviceConnection()){

            // Update the current gas type
            currentGasType = gasSensor.queryGasTypeCstr();
            if(strlen(currentGasType) <= 0){
                currentGasType = "INV_TYPE";
            }

            // Read the concentration
            currentConcentration = gasSensor.readGasConcentrationPPM();

            // And Temperature
            currentTemperature = gasSensor.readTempC();
        }
        else{
            ERROR(F("No acknowledge received from DFRobot Multi Gas Sensor."));
        }
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFMultiGasSensor::package() {
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json[currentGasType] = currentConcentration;
        json["Temp(C)"] = currentTemperature;

    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFMultiGasSensor::power_up() {
    FUNCTION_START;

    // If we are unable to connect to the sensor in power up this should disable the module for atleast this run, decide if we want to do this or not
    moduleInitialized = attemptConnectionToSensor();

    if(moduleInitialized){
        // Configure in passive mode with temperature compenstation off (default)
        configureSensorProperties();
        LOG(F("DFRobot Multi Gas sensor powered on successfully!"));
    }
    else{
        ERROR(F("DFRobot Multi Gas sensor failed to power on and has been disabled."));
    }
    
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_DFMultiGasSensor::attemptConnectionToSensor() {
    FUNCTION_START;

    /* Attempt a set number of times to initialize the sensor */
    for(uint8_t retryCount = 0; retryCount < retryLimit; retryCount++){
        LOGF("Attempting to connect to sensor... Attempt %u / %u ", retryCount+1, retryLimit+1);

        // If we do successfully begin the sensor we want to stop the loop immediatly and move on to the next part of initialization
        if(gasSensor.begin()){
            LOG(F("DFRobot Multi Gas Sensor connected! "));
            FUNCTION_END;
            return true;
        }
    
        // If we have reached the max number of retries, then we want to just stop and disable the module
        if(retryCount == retryLimit-1){
            ERRORF("Failed to connect to DFRobot Multi Gas Sensor after %u retries. ", retryLimit+1);
            FUNCTION_END;
            return false;
        }

        LOG(F("Waiting 3 seconds before attempting to retry..."));
        delay(3000);
    }

    // We shouldn't be able to make it here but if we do it was probably bad
    
    FUNCTION_END;
    return false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_DFMultiGasSensor::configureSensorProperties(DFRobot_GAS::eMethod_t aquireMode, DFRobot_GAS::eSwitch_t gasCompMode){
    // Set aquire mode to passive so we are able to request data from it whenever
        LOG(F("Setting Acquire Mode to..."));
        gasSensor.changeAcquireMode(gasSensor.PASSIVITY);
        delay(1000);
        LOGF("Acquire Mode set to %hs", aquireMode == gasSensor.PASSIVITY ? "PASSIVE" : "INITIATIVE");

        // Set temperature compensation
        LOG(F("Setting temp compensation..."));
        gasSensor.setTempCompensation(gasSensor.OFF);
        delay(1000);
        LOGF("Temp compensation set to %hs", gasCompMode == gasSensor.OFF ? "OFF" : "ON");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
