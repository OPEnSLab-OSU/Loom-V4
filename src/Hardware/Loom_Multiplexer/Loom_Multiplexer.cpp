#include "Loom_Multiplexer.h"
#include "Logger.h"
#include <Arduino.h>
//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::Loom_Multiplexer(Manager& man) : Module("Multiplexer"), manInst(&man), activeMuxAddr(0) {
    moduleInitialized = false;
    known_addresses = default_addresses;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::Loom_Multiplexer(Manager& man, const std::vector<byte>& addresses) : Module("Multiplexer"), manInst(&man), activeMuxAddr(0) {
    moduleInitialized = false;
    known_addresses = addresses.size() > 0 ? addresses : default_addresses;
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::~Loom_Multiplexer() {
    clearSensors();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setKnownAddresses(const std::vector<byte>& addresses) {
    FUNCTION_START;
    known_addresses = addresses.size() > 0 ? addresses : default_addresses;

    char output[OUTPUT_SIZE];
    snprintf(output, OUTPUT_SIZE, "Mux known address count set to %u", (unsigned int)known_addresses.size());
    debugLog(output);

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::enablePort(uint8_t port) {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if(port >= numPorts){
        snprintf(output, OUTPUT_SIZE, "Mux port %u is out of range", port);
        ERROR(output);
        debugLog(output);
        FUNCTION_END;
        return;
    }

    portEnabled[port] = true;
    snprintf(output, OUTPUT_SIZE, "Mux port %u enabled", port);
    debugLog(output);

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::disablePort(uint8_t port) {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if(port >= numPorts){
        snprintf(output, OUTPUT_SIZE, "Mux port %u is out of range", port);
        ERROR(output);
        debugLog(output);
        FUNCTION_END;
        return;
    }

    portEnabled[port] = false;
    snprintf(output, OUTPUT_SIZE, "Mux port %u disabled", port);
    debugLog(output);

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::disablePorts(const std::vector<uint8_t>& ports) {
    FUNCTION_START;

    for(uint8_t port : ports){
        disablePort(port);
    }

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::useOnlyPorts(const std::vector<uint8_t>& ports) {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    for(int i = 0; i < numPorts; i++){
        portEnabled[i] = false;
    }

    for(uint8_t port : ports){
        enablePort(port);
    }

    snprintf(output, OUTPUT_SIZE, "Mux scan restricted to %u requested port(s)", (unsigned int)ports.size());
    debugLog(output);

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setTSL2591Options(tsl2591Gain_t light_gain, tsl2591IntegrationTime_t integration_time) {
    FUNCTION_START;
    tsl2591Gain = light_gain;
    tsl2591IntegrationTime = integration_time;
    debugLog("TSL2591 auto-load options updated");
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setSEN66Options(bool measurePM, bool readNumVals) {
    FUNCTION_START;
    sen66MeasurePM = measurePM;
    sen66ReadNumVals = readNumVals;
    debugLog("SEN66 auto-load options updated");
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setDebug(bool enabled) {
    debugOutput = enabled;

    if(debugOutput){
        Serial.println(F("[MUX DEBUG] Serial debug enabled"));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::setScanDebug(bool enabled) {
    scanDebugOutput = enabled;

    if(debugOutput){
        Serial.print(F("[MUX DEBUG] Scan miss debug "));
        Serial.println(scanDebugOutput ? F("enabled") : F("disabled"));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::debugScan() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    byte previousMuxAddr = activeMuxAddr;
    bool previousInitialized = moduleInitialized;
    byte foundMuxAddr = 0;

    debugLog("Starting mux debug scan");
    Wire.begin();

    for(byte muxAddr : alt_addresses){
        snprintf(output, OUTPUT_SIZE, "Checking mux address 0x%02X", muxAddr);
        debugLog(output);

        uint8_t result = probeAddress(muxAddr);
        debugLogI2CResult("Mux address probe", muxAddr, result);

        if(result == 0 && probeMultiplexer(muxAddr)){
            foundMuxAddr = muxAddr;
            break;
        }
    }

    if(foundMuxAddr == 0){
        ERROR(F("Debug scan did not find a TCA9548 mux"));
        debugLog("Debug scan did not find a TCA9548 mux");
        FUNCTION_END;
        return;
    }

    activeMuxAddr = foundMuxAddr;
    moduleInitialized = true;

    snprintf(output, OUTPUT_SIZE, "Debug scan using mux address 0x%02X", activeMuxAddr);
    debugLog(output);

    if(known_addresses.size() <= 0){
        known_addresses = default_addresses;
        snprintf(output, OUTPUT_SIZE, "Known address list was empty, using default list of %u", (unsigned int)known_addresses.size());
        debugLog(output);
    }

    for(int port = 0; port < numPorts; port++){
        if(!isPortEnabled(port)){
            snprintf(output, OUTPUT_SIZE, "Debug scan skipping disabled mux port %i", port);
            debugLog(output);
            continue;
        }

        snprintf(output, OUTPUT_SIZE, "Debug scan selecting mux port %i", port);
        debugLog(output);
        if(!selectPin(port)){
            ERRORF("Failed to select mux port %i; skipping it.", port);
            continue;
        }
        delay(50);

        bool foundOnPort = false;

        for(byte addr : known_addresses){
            if(!shouldScanAddress(addr)){
                continue;
            }

            uint8_t result = probeAddress(addr);

            if(result == 0){
                foundOnPort = true;
                snprintf(output, OUTPUT_SIZE, "ACK on mux port %i at I2C address 0x%02X", port, addr);
                debugLog(output);
            }
            else if(scanDebugOutput){
                snprintf(output, OUTPUT_SIZE, "No ACK on mux port %i at I2C address 0x%02X, Wire error %u", port, addr, result);
                debugLog(output);
            }
        }

        if(!foundOnPort){
            snprintf(output, OUTPUT_SIZE, "No known devices found on mux port %i", port);
            debugLog(output);
        }
    }

    disableChannels();

    activeMuxAddr = previousMuxAddr;
    moduleInitialized = previousInitialized;

    debugLog("Finished mux debug scan");
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::initialize() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    Wire.begin();

    debugLog("Mux initialize entered");

    // Do not retain an old address or sensor list if a later re-scan fails.
    clearSensors();
    activeMuxAddr = 0;
    moduleInitialized = false;

    if(known_addresses.size() <= 0){
        known_addresses = default_addresses;
        snprintf(output, OUTPUT_SIZE, "Known address list was empty, using default list of %u", (unsigned int)known_addresses.size());
        debugLog(output);
    }
    else{
        snprintf(output, OUTPUT_SIZE, "Known address list has %u address(es)", (unsigned int)known_addresses.size());
        debugLog(output);
    }

    for(byte muxAddr : alt_addresses){
        snprintf(output, OUTPUT_SIZE, "Checking mux address 0x%02X", muxAddr);
        debugLog(output);

        uint8_t result = probeAddress(muxAddr);
        debugLogI2CResult("Mux address probe", muxAddr, result);

        if(result == 0 && probeMultiplexer(muxAddr)){
            snprintf(output, OUTPUT_SIZE, "Multiplexer found at address 0x%02X", muxAddr);
            LOG(output);
            debugLog(output);

            activeMuxAddr = muxAddr;
            moduleInitialized = true;

            scanAndLoadSensors();

            snprintf(output, OUTPUT_SIZE, "Mux initialization loaded %u sensor(s)", (unsigned int)sensors.size());
            debugLog(output);

            if(sensors.size() <= 0){
                ERROR(F("No sensors found!"));
                debugLog("No sensors found behind mux");
            }

            disableChannels();
            debugLog("Mux initialize finished");
            FUNCTION_END;
            return;
        }
    }

    ERROR(F("Multiplexer was not found at the standard address or any alternatives"));
    debugLog("Multiplexer was not found at 0x70-0x77");
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::refreshSensors() {
    FUNCTION_START;

    if(!moduleInitialized){
        ERROR(F("Cannot refresh sensors because multiplexer is not initialized"));
        debugLog("Cannot refresh sensors because mux is not initialized");
        FUNCTION_END;
        return;
    }

    debugLog("Refreshing mux sensors");
    clearSensors();
    scanAndLoadSensors();
    disableChannels();

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::clearSensors() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    snprintf(output, OUTPUT_SIZE, "Clearing %u auto-loaded mux sensor(s)", (unsigned int)sensors.size());
    debugLog(output);

    for(size_t i = 0; i < sensors.size(); i++){
        delete std::get<1>(sensors[i]);
    }

    sensors.clear();
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::scanAndLoadSensors() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if(known_addresses.size() <= 0){
        known_addresses = default_addresses;
        snprintf(output, OUTPUT_SIZE, "Known address list was empty, using default list of %u", (unsigned int)known_addresses.size());
        debugLog(output);
    }

    for(int port = 0; port < numPorts; port++){
        if(!isPortEnabled(port)){
            snprintf(output, OUTPUT_SIZE, "Skipping disabled mux port %i", port);
            LOG(output);
            debugLog(output);
            continue;
        }

        snprintf(output, OUTPUT_SIZE, "Scanning mux port %i", port);
        debugLog(output);

        if(!selectPin(port)){
            ERRORF("Failed to select mux port %i; skipping it.", port);
            continue;
        }
        delay(50);

        bool foundOnPort = false;

        for(byte addr : known_addresses){
            if(!shouldScanAddress(addr)){
                continue;
            }

            uint8_t result = probeAddress(addr);

            if(result == 0){
                foundOnPort = true;
                snprintf(output, OUTPUT_SIZE, "Found I2C device on port %i at address 0x%02X", port, addr);
                LOG(output);
                debugLog(output);

                Module* sensor = loadSensor(addr);

                if(sensor == nullptr){
                    snprintf(output, OUTPUT_SIZE, "No Loom sensor loader found for I2C address 0x%02X", addr);
                    ERROR(output);
                    debugLog(output);
                    continue;
                }

                snprintf(output, OUTPUT_SIZE, "%s_%i", sensor->getModuleName(), port);
                sensor->setModuleName(output);

                snprintf(output, OUTPUT_SIZE, "Initializing sensor %s", sensor->getModuleName());
                debugLog(output);

                sensor->initialize();

                if(!sensor->moduleInitialized){
                    snprintf(output, OUTPUT_SIZE,
                             "Sensor %s failed initialization and will not be loaded",
                             sensor->getModuleName());
                    ERROR(output);
                    delete sensor;
                    continue;
                }

                sensors.push_back(std::make_tuple(addr, sensor, port));

                snprintf(output, OUTPUT_SIZE, "Loaded sensor %s on port %i", sensor->getModuleName(), port);
                LOG(output);
                debugLog(output);
            }
            else if(scanDebugOutput){
                snprintf(output, OUTPUT_SIZE, "No ACK on mux port %i at I2C address 0x%02X, Wire error %u", port, addr, result);
                debugLog(output);
            }
        }

        if(!foundOnPort){
            snprintf(output, OUTPUT_SIZE, "No known devices found on mux port %i", port);
            debugLog(output);
        }
    }

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::measure() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if(!moduleInitialized){
        debugLog("Mux measure skipped because mux is not initialized");
        FUNCTION_END;
        return;
    }

    if(sensors.size() <= 0){
        debugLog("Mux measure skipped because no sensors are loaded");
    }

    for(size_t i = 0; i < sensors.size(); i++){
        if(!std::get<1>(sensors[i])->moduleInitialized)
            continue;
        snprintf(output, OUTPUT_SIZE, "Measuring mux sensor %s on port %i", std::get<1>(sensors[i])->getModuleName(), std::get<2>(sensors[i]));
        debugLog(output);

        if(!selectPin(std::get<2>(sensors[i])))
            continue;
        delay(50);
        std::get<1>(sensors[i])->measure();
    }

    disableChannels();
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::package() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if(!moduleInitialized){
        debugLog("Mux package skipped because mux is not initialized");
        FUNCTION_END;
        return;
    }

    if(sensors.size() <= 0){
        debugLog("Mux package skipped because no sensors are loaded");
    }

    for(size_t i = 0; i < sensors.size(); i++){
        if(!std::get<1>(sensors[i])->moduleInitialized)
            continue;
        snprintf(output, OUTPUT_SIZE, "Packaging mux sensor %s on port %i", std::get<1>(sensors[i])->getModuleName(), std::get<2>(sensors[i]));
        debugLog(output);

        if(!selectPin(std::get<2>(sensors[i])))
            continue;
        std::get<1>(sensors[i])->package();
    }

    disableChannels();
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::power_up() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if(!moduleInitialized){
        debugLog("Mux power_up skipped because mux is not initialized");
        FUNCTION_END;
        return;
    }

    for(size_t i = 0; i < sensors.size(); i++){
        if(!std::get<1>(sensors[i])->moduleInitialized &&
           !std::get<1>(sensors[i])->retryPowerUpWhenUninitialized())
            continue;
        snprintf(output, OUTPUT_SIZE, "Powering up mux sensor %s on port %i", std::get<1>(sensors[i])->getModuleName(), std::get<2>(sensors[i]));
        debugLog(output);

        if(!selectPin(std::get<2>(sensors[i])))
            continue;
        delay(50);
        std::get<1>(sensors[i])->power_up();
    }

    disableChannels();
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::power_down() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if(!moduleInitialized){
        debugLog("Mux power_down skipped because mux is not initialized");
        FUNCTION_END;
        return;
    }

    for(size_t i = 0; i < sensors.size(); i++){
        if(!std::get<1>(sensors[i])->moduleInitialized)
            continue;
        snprintf(output, OUTPUT_SIZE, "Powering down mux sensor %s on port %i", std::get<1>(sensors[i])->getModuleName(), std::get<2>(sensors[i]));
        debugLog(output);

        if(!selectPin(std::get<2>(sensors[i])))
            continue;
        delay(50);
        std::get<1>(sensors[i])->power_down();
    }

    disableChannels();
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::selectPin(uint8_t pin) {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if(pin >= numPorts){
        snprintf(output, OUTPUT_SIZE, "Cannot select mux port %u because it is out of range", pin);
        debugLog(output);
        FUNCTION_END;
        return false;
    }

    if(activeMuxAddr == 0){
        debugLog("Cannot select mux port because no mux address is active");
        FUNCTION_END;
        return false;
    }

    Wire.beginTransmission(activeMuxAddr);
    Wire.write(1 << pin);
    uint8_t result = Wire.endTransmission();

    if(debugOutput){
        snprintf(output, OUTPUT_SIZE, "Selected mux port %u with mask 0x%02X, Wire result %u", pin, (uint8_t)(1 << pin), result);
        debugLog(output);
    }

    FUNCTION_END;
    return result == 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::disableChannels() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];

    if(activeMuxAddr == 0){
        debugLog("Cannot disable mux channels because no mux address is active");
        FUNCTION_END;
        return false;
    }

    Wire.beginTransmission(activeMuxAddr);
    Wire.write(0);
    uint8_t result = Wire.endTransmission();

    if(debugOutput){
        snprintf(output, OUTPUT_SIZE, "Disabled all mux channels, Wire result %u", result);
        debugLog(output);
    }

    FUNCTION_END;
    return result == 0;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::isDeviceConnected(byte addr) {
    FUNCTION_START;

    bool response = probeAddress(addr) == 0;

    FUNCTION_END;
    return response;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
uint8_t Loom_Multiplexer::probeAddress(byte addr) {
    Wire.beginTransmission(addr);
    return Wire.endTransmission();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::probeMultiplexer(byte addr) {
    // A plain ACK is insufficient because Loom sensors also use 0x70-0x77.
    // A TCA9548 reads back its channel-control mask directly.
    if(Wire.requestFrom((int)addr, 1) != 1)
        return false;
    const uint8_t originalMask = Wire.read();

    const uint8_t testMasks[] = {0x00, 0x01};
    bool verified = true;
    for(uint8_t mask : testMasks){
        Wire.beginTransmission(addr);
        Wire.write(mask);
        if(Wire.endTransmission() != 0 || Wire.requestFrom((int)addr, 1) != 1 ||
           Wire.read() != mask){
            verified = false;
            break;
        }
    }

    // Restore whichever channel mask was active before the probe.
    Wire.beginTransmission(addr);
    Wire.write(originalMask);
    if(Wire.endTransmission() != 0)
        verified = false;

    return verified;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::isPortEnabled(uint8_t port) {
    FUNCTION_START;

    bool enabled = port < numPorts && portEnabled[port];

    FUNCTION_END;
    return enabled;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::shouldScanAddress(byte addr) {
    FUNCTION_START;

    bool shouldScan = addr > 0 && addr != activeMuxAddr;

    FUNCTION_END;
    return shouldScan;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::debugLog(const char* message) {
    if(debugOutput){
        Serial.print(F("[MUX DEBUG] "));
        Serial.println(message);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::debugLogI2CResult(const char* label, byte addr, uint8_t result) {
    if(!debugOutput){
        return;
    }

    if(result == 0 || scanDebugOutput){
        char output[OUTPUT_SIZE];
        snprintf(output, OUTPUT_SIZE, "%s 0x%02X: %s, Wire error %u", label, addr, result == 0 ? "ACK" : "NACK", result);
        debugLog(output);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Module* Loom_Multiplexer::loadSensor(const byte addr) {

    // Select the correct sensor to load based on the address.
    switch(addr){
        // TSL2591
        case 0x29: return new Loom_TSL2591(*manInst, 0x29, true, tsl2591Gain, tsl2591IntegrationTime);

        // ZX Gesture
        case 0x10: return new Loom_ZXGesture(*manInst, 0x10, true);
        case 0x11: return new Loom_ZXGesture(*manInst, 0x11, true);

        // SHT31
        case 0x44: return new Loom_SHT31(*manInst, 0x44, true);
        case 0x45: return new Loom_SHT31(*manInst, 0x45, true);

        // ADS1115
        case 0x48: return new Loom_ADS1115(*manInst, 0x48, true);

        // K30
        // case 0x68: return new Loom_K30(*manInst, true, 0x68, true);

        // MMA8451
        case 0x1C: return new Loom_MMA8451(*manInst, 0x1C, true);
        case 0x1D: return new Loom_MMA8451(*manInst, 0x1D, true);

        // Loom_DFMultiGasSensor
        case 0x74: return new Loom_DFMultiGasSensor(*manInst, 0x74, 10, true, true);
        case 0x75: return new Loom_DFMultiGasSensor(*manInst, 0x75, 10, true, true);

        // Loom_T6793
        case 0x15: return new Loom_T6793(*manInst, 0x15, 10, true);

        // MPU6050
        // case 0x69: return new Loom_MPU6050(*manInst, true);

        // SEN55
        case 0x69: return new Loom_SEN55(*manInst, true, true, true);

        // SEN66
        case 0x6B: return new Loom_SEN66(*manInst, sen66MeasurePM, true, sen66ReadNumVals);

        // MS5803
        case 0x76: return new Loom_MS5803(*manInst, 0x76, true);
        case 0x77: return new Loom_MS5803(*manInst, 0x77, true);

        // STEMMA
        case 0x36: return new Loom_STEMMA(*manInst, 0x36, true);

        // MB1232
        case 0x70: return new Loom_MB1232(*manInst, 0x70, true);

        default: return nullptr;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
