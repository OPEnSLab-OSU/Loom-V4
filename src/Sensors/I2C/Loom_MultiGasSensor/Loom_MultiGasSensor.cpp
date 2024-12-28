#include "Loom_MultiGasSensor.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MultiGasSensor::Loom_MultiGasSensor(
                            Manager& man,
                            int address,
                            bool useMux
                    ) : I2CDevice("MultiGasSensor"), manInst(&man), gas(&Wire, address) {
                        module_address = address;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MultiGasSensor::initialize() {
    FUNCTION_START;
    if(!gas.begin()){
        ERROR(F("Failed to initialize Gas Sensor! Check connections and try again..."));
        moduleInitialized = false;
    }
    else{
        gas.changeAcquireMode(gas.PASSIVITY);
        gas.setTempCompensation(gas.ON);

        LOG(F("Successfully initialized Gas Sensor!"));
        moduleInitialized = true;
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MultiGasSensor::measure() {
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
            ERROR(F("No acknowledge received from the device"));
            FUNCTION_END;
            return;
        }

       
        // Pull the data from the sensor
        gasType = gas.queryGasType();
        Concentration = gas.readGasConcentrationPPM();
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MultiGasSensor::package() {
    FUNCTION_START;
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["Gas Type"] = gasType;
        json["Gas Concentrqation (PPM)"] = Concentration;
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////



