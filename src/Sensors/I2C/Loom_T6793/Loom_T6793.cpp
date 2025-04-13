#include "Loom_T6793.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_T6793::Loom_T6793(
                        Manager& man,
                        uint8_t addr,
                        uint8_t readDelay, // Delay for reading Wire in ms
                        bool useMux
                    ) : I2CDevice("T6793"), manInst(&man), i2s_addr(addr), wireReadDelay(readDelay){

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_T6793::initialize() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char errorMessage[OUTPUT_SIZE];

    /* Initialize wire and start the sensor using the standard I2C interface */
    Wire.begin();

    // Wire init delay
    unsigned long startMillis = millis();
    while((millis() - startMillis) < 1000) {}


    if(this->GetSensorStatus()){
        LOG("Sensor successfully initialized!");
    }
    else{
        snprintf(output, OUTPUT_SIZE, "Error occurred while attempting to reset device, module will not be initialized!");
        ERROR(output);
        moduleInitialized = false;
    }

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_T6793::measure() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char sensorError[OUTPUT_SIZE];

    byte data[4];
    Wire.beginTransmission(i2s_addr);

    Wire.write(0x04); 
    Wire.write(0x13); 
    Wire.write(0x8B); 
    Wire.write(0x00); 
    Wire.write(0x01);
    
    if (Wire.endTransmission() != 0) { CO2_Val = -1; }

    // Read delay
    unsigned long startMillis = millis();
    while((millis() - startMillis) < wireReadDelay) {}

    Wire.requestFrom(i2s_addr, 4);
    for (int i = 0; i < 4; i++) {
        if (Wire.available()) data[i] = Wire.read();
        else { CO2_Val = -1; }
    }

    CO2_Val = ((data[2] & 0x3F) << 8) | data[3];

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_T6793::package() {
    FUNCTION_START;
    JsonObject json = manInst->get_data_object(getModuleName());

    json["CO2"] = CO2_Val;

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_T6793::GetSensorStatus() {
    FUNCTION_START;
    byte data[4];
    Wire.beginTransmission(i2s_addr);

    Wire.write(0x04); 
    Wire.write(0x13); 
    Wire.write(0x8A); 
    Wire.write(0x00); 
    Wire.write(0x01);

    if (Wire.endTransmission() != 0) return false;

    // Read delay
    unsigned long startMillis = millis();
    while((millis() - startMillis) < wireReadDelay) {}

    // delay(readDelay);
    Wire.requestFrom(i2s_addr, 4);
    for (int i = 0; i < 4; i++) {
        if (Wire.available()) data[i] = Wire.read();
        else return false;
    }

    FUNCTION_END;
    return ((data[2] & 0x01) == 0);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

unsigned long Loom_T6793::GetSerialNo() {

    FUNCTION_START;
    byte data[6];
    Wire.beginTransmission(i2s_addr);

    Wire.write(0x03); 
    Wire.write(0x0F); 
    Wire.write(0xA2); 
    Wire.write(0x00); 
    Wire.write(0x02);

    if (Wire.endTransmission() != 0) {
        Serial.println("Serial Request Failed!");
        return 0;
    }

    // Read delay
    unsigned long startMillis = millis();
    while((millis() - startMillis) < wireReadDelay) {}

    // delay(readDelay);
    Wire.requestFrom(i2s_addr, 6);
    for (int i = 0; i < 6; i++) {
        if (Wire.available()) data[i] = Wire.read();
        else return 0;
    }
    FUNCTION_END;

    return ((unsigned long)data[2] << 24) | ((unsigned long)data[3] << 16) | ((unsigned long)data[4] << 8) | (unsigned long)data[5];
}