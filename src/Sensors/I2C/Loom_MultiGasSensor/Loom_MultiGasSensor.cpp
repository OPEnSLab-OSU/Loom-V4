#include "Loom_MultiGasSensor.h"
#include "Wire.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_MultiGasSensor::Loom_MultiGasSensor(
                            Manager& man,
                            uint8_t address,
                            bool useMux
                    ) : I2CDevice("MultiGasSensor"), manInst(&man), addr(address), gas(&Wire, address) {
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

    char output[OUTPUT_SIZE];

    LOG(F("Scanning I2C for MultiGasSensor..."));

    addr = findGasBoard();
    if (addr == -1) {
        ERROR(F("No gas board found on this channel."));
        moduleInitialized = false;
        return;
    }
    
    // addr = scanI2C();
    // gas.setI2cAddr(addr);
    // module_address = addr;
    // LOG(F("I2C address set"));

    // uint8_t addr = *maybeAddr;
    gas.setI2cAddr(addr);                     // fine, or skip if ctor already had it
    module_address = addr;


    int retries = 0;
    moduleInitialized = true;
    while (!gas.begin()) {

        snprintf(output, OUTPUT_SIZE, "Gas sensor present at 0x%02X but did not initialise. Retrying...", addr);
        ERROR(output);
        moduleInitialized = false;
        delay(1000);
        retries++;
        if(retries > DF_MAX_RETRIES){
            LOG(F("Failed to initialize Gas Sensor... Check connections and try again!"));
            break;
        }
        moduleInitialized = true;
    }


    // int retries = 0;
    // moduleInitialized = true;
    // while (!gas.begin()){
    //     LOG(F("Failed to initialize Gas Sensor! Retrying..."));
    //     moduleInitialized = false;
    //     delay(1000);
    //     retries++;
    //     if(retries > DF_MAX_RETRIES){
    //         LOG(F("Failed to initialize Gas Sensor... Check connections and try again!"));
    //         break;
    //     }
    //     moduleInitialized = true;
    // }
    LOG(F("Gas sensor initialized"));

    if(moduleInitialized){
        LOG(F("Setting acquire mode"));
        gas.changeAcquireMode(gas.PASSIVITY);
        delay(1000);
        LOG(F("Acquire mode set to PASSIVITY"));

        LOG(F("Setting temp compensation"));
        gas.setTempCompensation(gas.OFF);
        LOG(F("Temp compensation set to OFF"));

        LOG(F("Successfully initialized Gas Sensor!"));
    }
    else{
        LOG(F("Gas sensor: Failed to initialize"));
    }

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_MultiGasSensor::measure() {
    FUNCTION_START;

    char output[OUTPUT_SIZE];

    if(moduleInitialized){
        LOG(F("Checking device connection"));
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
            // gas.changeI2cAddrGroup(get_gas_i2c(type));
            queryResult = gas.queryGasType();
            
            snprintf(output, OUTPUT_SIZE, "queryResult: %s for gas type: %s", queryResult.c_str(), type.c_str());
            LOG(output);

            if (queryResult.length() > 0 && queryResult == String(type.c_str())) {
                float concentration = gas.readGasConcentrationPPM();
                snprintf(output, OUTPUT_SIZE, "queryResult: %s , concentration: %f", queryResult.c_str(), concentration);
                LOG(output);
                if (concentration >= 0.0) {
                    (*gasData)[type] = concentration;
                }
            }
        }

        float temp = gas.readTempC();
        (*gasData)["Temperature(C)"] = temp;
    }
    else{
        LOG(F("Gas sensor: Module not initialized"));
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
            json[i->first] = String(i->second);
        }
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


uint8_t findGasBoard(void)
{

char output[OUTPUT_SIZE];
  for (uint8_t addr = 0x70; addr <= 0x77; ++addr) {   // only range that matters
    Wire.beginTransmission(addr);
    if (Wire.endTransmission() == 0) {
        snprintf(output, OUTPUT_SIZE, "Found addr: %x", addr);
        LOG(output);
        return addr;     // found it
    }
  }
  return -1;                                // none found
}