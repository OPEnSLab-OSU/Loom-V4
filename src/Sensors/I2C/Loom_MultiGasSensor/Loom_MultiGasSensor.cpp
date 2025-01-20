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

uint8_t Loom_MultiGasSensor::get_gas_i2c(const std::string gasType){
    if(gasType == "O2")
        return 0x05;
    if (gasType == "CO")
        return 0x04;
    if (gasType == "H2S")
        return 0x03;
    if (gasType == "NO2")
        return 0x2C;
    if (gasType == "O3")
        return 0x2A;
    if (gasType == "CL2")
        return 0x31;
    if (gasType == "NH3")
        return 0x02;
    if (gasType == "H2")
        return 0x06;
    if (gasType == "HCL")
        return 0x2E;
    if (gasType == "SO2")
        return 0x2B;
    if (gasType == "HF")
        return 0x33;
    if (gasType == "PH3")
        return 0x45;
    return 0x00;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MultiGasSensor::initialize() {
    FUNCTION_START;
    int retries = 0;
    gas.setI2cAddr(0x05);

    while (!gas.begin()){
        ERROR(F("Failed to initialize Gas Sensor! Retrying..."));
        moduleInitialized = false;
        delay(1000);
        retries++;
        if(retries > DF_MAX_RETRIES){
            ERROR(F("Failed to initialize Gas Sensor... Check connections and try again!"));
            break;
        }
        moduleInitialized = true;
    }

    if(moduleInitialized){
        gas.setTempCompensation(gas.ON);
        gas.changeAcquireMode(gas.PASSIVITY);
        delay(1000);
        LOG(F("Successfully initialized Gas Sensor!"));
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
        if (gasData != nullptr){
            delete gasData;
            gasData = nullptr;
        }

        gasData = new std::unordered_map<std::string, float>;

        gasData->insert({"CO", 0.0});
        gasData->insert({"O2", 0.0});
        gasData->insert({"NH3", 0.0});
        gasData->insert({"H2S", 0.0});
        gasData->insert({"NO2", 0.0});
        gasData->insert({"HCL", 0.0});
        gasData->insert({"H2", 0.0});
        gasData->insert({"PH3", 0.0});
        gasData->insert({"SO2", 0.0});
        gasData->insert({"O3", 0.0});
        gasData->insert({"CL2", 0.0});
        gasData->insert({"HF", 0.0});

        for (auto i = gasData->begin(); i != gasData->end(); ++i){
            const std::string currentGas = i->first;
            gas.changeI2cAddrGroup(get_gas_i2c(currentGas));
            delay(50);

            if (gas.queryGasType() == String(i->first.c_str())){
                i->second = gas.readGasConcentrationPPM();
            }
            else{
                ERROR(F("Gas type was set but does not query correctly"));
            }
        }
    gasData->insert({"Temperature(C)", gas.readTempC()});
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MultiGasSensor::package() {
    FUNCTION_START;
    if(moduleInitialized && gasData != nullptr){
        JsonObject json = manInst->get_data_object(getModuleName());
        for (auto i = gasData->begin(); i != gasData->end(); ++i){
            json[i->first] = i->second;
        }
        delete gasData;
        gasData = nullptr;
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
