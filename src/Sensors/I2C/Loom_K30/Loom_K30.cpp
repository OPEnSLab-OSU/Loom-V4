#include "Loom_K30.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_K30::Loom_K30(Manager& man, int addr, bool warmUp, int valMult) : Module("K30"), manInst(&man), valMult(valMult), warmUp(warmUp), addr(addr){
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_K30::initialize(){
    Wire.begin();

    // Check if the sensor is actually connected
    Wire.beginTransmission(addr);
    if(Wire.endTransmission() != 0){
        printModuleName(); Serial.println("Failed to initialize sensor!");
        moduleInitialized = false;
        return;
    }

    // If we want to wait for the sensor to warmup do so here
    if(warmUp){
        printModuleName(); Serial.println("Warm-up was enabled for this sensor. Initialization will now pause for 6 minutes");

        // Pause for 6 minutes
        manInst->pause(60000 * 6);
    }

    printModuleName(); Serial.println("Initialized successfully!");
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_K30::measure(){
    
    // Send the request for data
    Wire.beginTransmission(addr);
    Wire.write(0x22);
    Wire.write(0x00);
    Wire.write(0x08);
    Wire.write(0x2A);
    Wire.endTransmission();

    // Wait to ensure data is properly recorded
    delay(10);

    // Request 4 bytes of data from the sensor
    Wire.requestFrom(addr, 4);

    // Read the data from the wire
    for(int i = 0; i < 4; i++){
        buffer[i] = Wire.read();
    }

    // Make sure the data is correct
    if(verifyChecksum()){
        // Bit shift the result into an integer
        CO2Levels = 0;
        CO2Levels |= buffer[1] & 0xFF;
        CO2Levels = CO2Levels << 8;
        CO2Levels |= buffer[2] & 0xFF;
    }
    else{
        printModuleName(); Serial.println("Failed to validate checksum! Using previously recorded data.");
    }

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_K30::package(){
    JsonObject json = manInst->get_data_object(getModuleName());
    json["CO2 Level"] = CO2Levels;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_K30::verifyChecksum(){
    // Verify the data is actually correct
    byte sum = 0;
    sum = buffer[0] + buffer[1] + buffer[2]; 
    return sum == buffer[3];
}
//////////////////////////////////////////////////////////////////////////////////////////////////////