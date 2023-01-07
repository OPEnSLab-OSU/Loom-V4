#include "Loom_EZODO.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_EZODO::Loom_EZODO(Manager& man, byte address, bool useMux) : I2CDevice("EZO-DO"), manInst(&man), i2c_address(address){
    module_address = i2c_address;

    if(!useMux)
        manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZODO::initialize(){
    Wire.begin();
    moduleInitialized = calibrate();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZODO::measure(){
    if(moduleInitialized){

        // Clear the sensorData received previously
        sensorData = "";

        // Check that the device is still present and connected
        if(!checkDeviceConnection()){
            printModuleName(); Serial.println("Failed to detect device at the specified address");
            return;
        }

        // Attempt to send a read command to the device
        if(!sendTransmission("r")){
            printModuleName(); Serial.println("Failed to send 'read' command to device");
            return;
        }
        
        // Wait the desired warm-up period
        delay(575);  

        // Request 32 bytes of data from the device
        Wire.requestFrom(i2c_address, 32, 1);

        // Check if the I2C code was not valid
        code = Wire.read();
        if(code != 1){
            printModuleName(); Serial.println("Unsuccessful Response Code Received: " + responseCodes[code-1]);
            return;
        } 

        // Read out only the next 32 bytes
        for(int i = 0; i < 32; i++){
            currentChar = Wire.read();

            // If a null char was received break out of the loop
            if(currentChar == 0) break;

            // If not append the current char to the string
            sensorData += currentChar;
        }

        // Parse the constructed string
        parseResponse();
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZODO::package(){
    if(moduleInitialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        json["D-Ox"] = oxygen;
        json["Sat"] = saturation;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZODO::power_down(){
    if(moduleInitialized){
       if(!sendTransmission("sleep")){
            printModuleName(); Serial.println("Failed to send 'sleep' command to device");
       }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_EZODO::calibrate(){

    // Check the device is connected before calibrating
    if(!checkDeviceConnection()){
        printModuleName(); Serial.println("Failed to detect device at the specified address");
        return false;
    }

    // Send the calibrate command
    if(sendTransmission("c") != 0){
        printModuleName(); Serial.println("Failed to transmit calibration command");
        return false;
    }

    // Wait calibration time
    delay(575);

    printModuleName(); Serial.println("Device successfully calibrated!");

    return true;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_EZODO::sendTransmission(String command){
    Wire.beginTransmission(i2c_address);
    Wire.write(command.c_str());

    // Use a ternary operator to ensure if it is 0 its true if not we are false
    return Wire.endTransmission() == 0 ? true : false;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_EZODO::parseResponse(){
    int commaIndex = sensorData.indexOf(",");
    oxygen = sensorData.substring(0, commaIndex).toFloat();
    saturation = sensorData.substring(commaIndex+1, sensorData.length()).toFloat();
}
//////////////////////////////////////////////////////////////////////////////////////////////////////