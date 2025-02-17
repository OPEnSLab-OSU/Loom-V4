#include "Loom_MultiGasSensor.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MultiGasSensor::Loom_MultiGasSensor(
                            Manager& man,
                            uint8_t address,
                            bool useMux
                    ) : I2CDevice("MultiGasSensor"), manInst(&man), addr(address) {
                        module_address = address;

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t Loom_MultiGasSensor::get_gas_i2c(const std::string& gasType) {
    static const std::unordered_map<std::string, uint8_t> GAS_ADDRESSES = {
        {"O2",  0x05},
        {"CO",  0x04},
        {"H2S", 0x03},
        {"NO2", 0x2C},
        {"O3",  0x2A},
        {"CL2", 0x31},
        {"NH3", 0x02},
        {"H2",  0x06},
        {"HCL", 0x2E},
        {"SO2", 0x2B},
        {"HF",  0x33},
        {"PH3", 0x45}
    };

    auto it = GAS_ADDRESSES.find(gasType);
    return (it != GAS_ADDRESSES.end()) ? it->second : 0x00;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MultiGasSensor::initialize() {
    FUNCTION_START;

    gas.setI2cAddr(addr);
    delay(100);

    // If scan is necessary
    if (!checkDeviceConnection()){
        LOG(F("Scanning I2C for MultiGasSensor..."));
        addr = scanI2C();
        gas.setI2cAddr(addr);
        module_address = addr;
        needsReinit = false;
        char *msg = (char*)malloc(100);
        snprintf_P(msg, 100, PSTR("MultiGasSensor addr changed to: 0x%02X"), addr);
        LOG(msg);
        free(msg);
    }
    else{
        LOG(F("MultiGasSensor connection found, no scan necessary!"));
    }

    int retries = 0;
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
        bool connectionStatus = checkDeviceConnection();
        if(connectionStatus && needsReinit){
            initialize();
            needsReinit = false;
        }
        else if(!connectionStatus){
            ERROR(F("No acknowledge received from the device"));
            FUNCTION_END;
            return;
        }

        const std::vector<std::string> gasTypes = {
            "O2", "CO", "H2S", "NO2", "O3", "CL2",
            "NH3", "H2", "HCL", "SO2", "HF", "PH3"
        };

        if (gasData == nullptr) {
            gasData = new std::unordered_map<std::string, float>;
        }

        String queryResult;
        for(const auto& type : gasTypes) {
            (*gasData)[type] = 0.0;
            gas.changeI2cAddrGroup(get_gas_i2c(type));
            delay(50);

            queryResult = gas.queryGasType();
            if (queryResult.length() > 0 && queryResult == String(type.c_str())) {
                float concentration = gas.readGasConcentrationPPM();
                if (concentration >= 0.0) {
                    (*gasData)[type] = concentration;
                }
            }
        }

        float temp = gas.readTempC();
        (*gasData)["Temperature(C)"] = temp;
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
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
