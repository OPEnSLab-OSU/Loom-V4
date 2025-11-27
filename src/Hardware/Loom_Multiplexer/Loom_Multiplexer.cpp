#include "Loom_Multiplexer.h"
#include "Logger.h"
//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::Loom_Multiplexer(Manager& man) : Module("Multiplexer"), manInst(&man) {
    moduleInitialized = false;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::Loom_Multiplexer(Manager& man, const std::vector<byte>& addresses) : Module("Multiplexer"), manInst(&man){
    moduleInitialized = false;
    manInst->registerModule(this);
    known_addresses = addresses;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::~Loom_Multiplexer()  {
    for(int i = 0; i < sensors.size(); i++){
        delete std::get<1>(sensors[i]);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::initialize(){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    Wire.begin();
    int moduleIndex = 0;

    // set known addresses to addresses from SD
    if(config_addresses.size() > 0)
        known_addresses = config_addresses;

    // Find the multiplexer at the possible addresses
    for(byte addr : alt_addresses){

        // Check if there is a device on the current I2C device
        if(isDeviceConnected(addr)) {
            snprintf(output, OUTPUT_SIZE, "Multiplexer found at address %x", addr);
            LOG(output);
            activeMuxAddr = addr;
            moduleInitialized = true;

            moduleIndex = 0;
            // Load the sensors for the first time
            for(int i = 0; i < numPorts; i++){
                selectPin(i);

                // Loop over address that we know to speed up refreshing
                for(byte addr : known_addresses){

                    if(addr > 0){
                        // Is there any device at this address
                        if(isDeviceConnected(addr)){
                            snprintf(output, OUTPUT_SIZE, "Found I2C Device on Pin %i at address %x", i, addr);
                            LOG(output);

                            sensors.push_back(std::make_tuple(addr, loadSensor(addr), i));

                            // Initialize connected sensor
                            snprintf(output, OUTPUT_SIZE, "%s_%i", std::get<1>(sensors[moduleIndex])->getModuleName(), i);
                            std::get<1>(sensors[moduleIndex])->setModuleName(output);
                            std::get<1>(sensors[moduleIndex])->initialize();

                            snprintf(output, OUTPUT_SIZE, "Loaded sensor %s on port %i", std::get<1>(sensors[moduleIndex])->getModuleName(), i);
                            LOG(output);
                            moduleIndex++;
                        }
                    }
                }
            }

            if(sensors.size() <= 0){
                ERROR(F("No sensors found!"));
            }
            FUNCTION_END;
            return;
        }
    }

    // There was no device so error
    ERROR(F("Multiplexer was not found at the standard address or any alternatives"));
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::refreshSensors(){
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    int moduleIndex = 0;

    // Cycle through the mux ports
    for(int i = 0; i < numPorts; i++){
        selectPin(i);
        delay(50);
       
        // Loop over address that we know to speed up refreshing
        for(byte addr : known_addresses){

            // Is there any device at this address
            if(isDeviceConnected(addr) && addr > 0){

                // If it was a new sensor plugged in then load a new sensor over top
                if(std::get<1>(sensors[moduleIndex])->module_address != addr){
                    delete std::get<1>(sensors[moduleIndex]);
                    sensors[moduleIndex] = std::make_tuple(addr, loadSensor(addr), i);

                    // Initialize the new sensor
                    snprintf(output, OUTPUT_SIZE, "New sensor detected on port %i at I2C address %x of type %s", i, addr, std::get<1>(sensors[moduleIndex])->getModuleName());
                    LOG(output);
                    
                    // Update name for unique instances in the Mux
                    snprintf(output, OUTPUT_SIZE, "%s_%i", std::get<1>(sensors[moduleIndex])->getModuleName(), i);
                    std::get<1>(sensors[moduleIndex])->setModuleName(output);
                    std::get<1>(sensors[moduleIndex])->initialize();
                }
                moduleIndex++;
            }
        }
        
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::measure(){
    FUNCTION_START;

    // Refresh sensors before measuring
    // refreshSensors();

    for(int i = 0; i < sensors.size(); i++){
        selectPin(std::get<2>(sensors[i]));
        delay(50);
        std::get<1>(sensors[i])->measure();
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::package(){
    FUNCTION_START;
    for(int i = 0; i < sensors.size(); i++){
        std::get<1>(sensors[i])->package();
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::power_up(){
    FUNCTION_START;
    for(int i = 0; i < sensors.size(); i++){
        selectPin(std::get<2>(sensors[i]));
        delay(50);
        std::get<1>(sensors[i])->power_up();
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::power_down(){
    FUNCTION_START;
    for(int i = 0; i < sensors.size(); i++){
        selectPin(std::get<2>(sensors[i]));
        delay(50);
        std::get<1>(sensors[i])->power_down();
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::selectPin(uint8_t pin){
    FUNCTION_START;
    // Pin not in range
    if(pin > 7) return;

    Wire.beginTransmission(activeMuxAddr);
    Wire.write(1 << pin);
    Wire.endTransmission();
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::disableChannels(){
    FUNCTION_START;
    Wire.beginTransmission(activeMuxAddr);
    Wire.write(0);
    Wire.endTransmission();
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::isDeviceConnected(byte addr){
    FUNCTION_START;
    Wire.beginTransmission(addr);

    // If end transmission returns 0 there is a device there
    bool response = Wire.endTransmission() == 0;
    FUNCTION_END;
    return response;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Module* Loom_Multiplexer::loadSensor(const byte addr){
    // Select the correct sensor to load based on the address
    switch (addr){
        //TSL2591
        case 0x29: return new Loom_TSL2591(*manInst, 0x29, true);

        // ZX Gesture
        case 0x10: return new Loom_ZXGesture(*manInst, 0x10, true);
        case 0x11: return new Loom_ZXGesture(*manInst, 0x11, true);

        // SHT31
        case 0x44: return new Loom_SHT31(*manInst, 0x44, true);
        case 0x45: return new Loom_SHT31(*manInst, 0x45, true);

        // ADS1115  
        case 0x48: return new Loom_ADS1115(*manInst, 0x48, true);

        // K30 - 
        case 0x68: return new Loom_K30(*manInst, true, 0x68, true);

        //MMA8451
        case 0x1D: return new Loom_MMA8451(*manInst, 0x1D, true);

        //Loom_DFMultiGasSensor
        case 0x74: return new Loom_DFMultiGasSensor(*manInst, 0x74, 10,false, true);
        case 0x75: return new Loom_DFMultiGasSensor(*manInst, 0x75, 10,false, true);

        // Loom_T6793
        case 0x15: return new Loom_T6793(*manInst, 0x15, 10, true);

        // MPU6050
        // case 0x69: return new Loom_MPU6050(*manInst,  true);

        // SEN55
        case 0x69: return new Loom_SEN55(*manInst,0x69, true);

        // MS5803
        case 0x76: return new Loom_MS5803(*manInst, 0x76, true);
        case 0x77: return new Loom_MS5803(*manInst, 0x77, true);

        // STEMMA
        case 0x36: return new Loom_STEMMA(*manInst, 0x36, true);

        /// MB1232
        case 0x70: return new Loom_MB1232(*manInst, 0x70, true);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////

void Loom_Multiplexer::loadConfigFromSD(const char* fileName){
    FUNCTION_START;
    // Doc to store the JSON data from the SD card in
    StaticJsonDocument<OUTPUT_SIZE> doc;
    char output[OUTPUT_SIZE];
    char* fileRead = sdMan->readFile(fileName);
    // avoid zero-copy behavior
    DeserializationError deserialError = deserializeJson(doc, (const char *)fileRead);
    free(fileRead);

    // Create JsonArray object to store sensor array
    JsonArray sensorMap = doc["sensors"];
    

    if(deserialError != DeserializationError::Ok){
        snprintf(output, OUTPUT_SIZE, "There was an error reading the config from SD: %s, multiplexer will use default_addresses", deserialError.c_str());
        ERROR(output);
    }
    else{
            LOG(F("Config successfully loaded from SD!"));
            if(!sensorMap.isNull()){
                //reserve space in config_addresses before loop
                config_addresses.reserve(sensorMap.size());
                // just takes addresses and pushes them onto config_addresses. Could also display what sensors were using if wanted. 
                for(int i = 0; i < sensorMap.size(); i++){
                    config_addresses.push_back(sensorMap[i]["addr"]);
                }
                snprintf(output, OUTPUT_SIZE, "Retrieved %u addresses from SD.", config_addresses.size());
                LOG(output);
            }

            // If the sensors array is empty or null, use default_addresses. 
            else{
                snprintf(output, OUTPUT_SIZE, "There was an error retrieving the sensor addresses from the JSON document, default_address's will be used");
                // known addresses empty vector by default, set if it has not been set already. 
                if(!(known_addresses.size() > 0))
                    known_addresses = default_addresses;
                ERROR(output);
            }
        }
}