#include "Loom_Multiplexer.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::Loom_Multiplexer(Manager& man) : Module("Multiplexer"), manInst(&man) {
    moduleInitialized = false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_Multiplexer::~Loom_Multiplexer()  {
    for(int i = 0; i < sensors.size(); i++){
        delete sensors[i].second;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::initialize(){
    Wire.begin();
    
    // Find the multiplexer at the possible addresses
    for(byte addr : alt_addresses){

        // Check if there is a device on the current I2C device
        if(isDeviceConnected(addr)) {
            printModuleName(); Serial.println("Multiplexer found at address: " + String(addr));
            activeMuxAddr = addr;
            moduleInitialized = true;

            // Load the sensors for the first time
            for(int i = 0; i < numPorts; i++){
                selectPin(i);

                // Loop over address that we know to speed up refreshing
                for(byte addr : known_addresses){

                    // Is there any device at this address
                    if(isDeviceConnected(addr)){
                        sensors.push_back(std::make_pair(addr, loadSensor(addr)));

                        // Initialize connected sensor
                        sensors[i].second->setModuleName(sensors[i].second->getModuleName() + String(i));
                        sensors[i].second->initialize();
                        printModuleName(); Serial.println("Loaded sensor " + sensors[i].second->getModuleName() + " on port " + String(i));
                    }
                }
            }
            return;
        }
    }

    // There was no device so error
    printModuleName(); Serial.println("Multiplexer was not found at the standard address or any alternatives");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::refreshSensors(){

    // Cycle through the mux ports
    for(int i = 0; i < numPorts; i++){
        selectPin(i);
       
        // Loop over address that we know to speed up refreshing
        for(byte addr : known_addresses){
            // Is there any device at this address
            if(isDeviceConnected(addr)){

                // If it was a new sensor plugged in then load a new sensor over top
                if(sensors[i].first != addr){
                    delete sensors[i].second;
                    sensors[i] = std::make_pair(addr, loadSensor(addr));

                    // Initialize the new sensor
                    printModuleName(); Serial.println("New sensor detected on port " + String(i) + " at I2C address " + String(addr) + " of type " + sensors[i].second->getModuleName());
                    
                    // Update name for unique instances in the Mux
                    sensors[i].second->setModuleName(sensors[i].second->getModuleName() + String(i));
                    sensors[i].second->initialize();
                }
            }
        }
        
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::measure(){

    // Refresh sensors before measuring
    refreshSensors();

    for(int i = 0; i < sensors.size(); i++){
        sensors[i].second->measure();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::package(){
    for(int i = 0; i < sensors.size(); i++){
        sensors[i].second->package();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::power_up(){
    for(int i = 0; i < sensors.size(); i++){
        sensors[i].second->power_up();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::power_down(){
    for(int i = 0; i < sensors.size(); i++){
        sensors[i].second->power_up();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::selectPin(uint8_t pin){
    // Pin not in range
    if(pin > 7) return;

    Wire.beginTransmission(activeMuxAddr);
    Wire.write(1 << pin);
    Wire.endTransmission();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Multiplexer::disableChannels(){
    Wire.beginTransmission(activeMuxAddr);
    Wire.write(0);
    Wire.endTransmission();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_Multiplexer::isDeviceConnected(byte addr){
    Wire.beginTransmission(addr);

    // If end transmission returns 0 there is a device there
    return (Wire.endTransmission() == 0);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
Module* Loom_Multiplexer::loadSensor(const byte addr){

    // Select the correct sensor to load based on the address
    switch (addr){
        //TSL2591
        case 0x29: return new Loom_TSL2591(*manInst);

        // SHT31
        case 0x44: return new Loom_SHT31(*manInst);
        case 0x45: return new Loom_SHT31(*manInst);

        // ADS1115  
        case 0x48: return new Loom_ADS1115(*manInst);

        // MPU6050
        case 0x68: return new Loom_MPU6050(*manInst);
        case 0x69: return new Loom_MPU6050(*manInst);

        case 0x76: return new Loom_MS5803(*manInst);
        case 0x77: return new Loom_MS5803(*manInst);

        case 0x36: return new Loom_STEMMA(*manInst);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////