#include "Loom_Analog.h"

namespace {
    constexpr uint8_t BATTERY_SAMPLE_COUNT = 8;
    constexpr float ADC_REFERENCE_VOLTAGE = 3.3f;
    constexpr float ADC_MAX_READING = 4095.0f;
    constexpr float BATTERY_DIVIDER_SCALE = 2.0f;
}


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Analog::measure(){

    // Read the data from the given analog pin
    for(size_t i = 0; i < pinMappings.size(); i++){

        /* If we are measuring the Vbat pin we want a little different behavior */
        if(pinMappings[i]->pinNumber == A7){
            const float batteryVoltage = getBatteryVoltage();
            pinMappings[i]->analog = batteryVoltage;
            pinMappings[i]->analog_mv = batteryVoltage * 1000.0f;
        }

        /* If its a normal pin then just read the value and update the previous values */
        else{
            int analogData = analogRead(pinMappings[i]->pinNumber);
            pinMappings[i]->analog = analogData;
            pinMappings[i]->analog_mv = analogToMV(analogData);
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Analog::package(){
    char output[10];
    JsonObject json = manInst->get_data_object(getModuleName());

    /* Loop over the list of pins and pull out the data to formulate the JSON entries*/
    for(size_t i = 0; i < pinMappings.size(); i ++){
        memset(output, '\0', 10);
        json[pinMappings[i]->name] = pinMappings[i]->analog;

        /* Append MV to the name to differentiate between normal analog and the millivolt representation */
        snprintf(output, sizeof(output), "%s_MV", pinMappings[i]->name);
        json[output] = pinMappings[i]->analog_mv;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::getBatteryVoltage(){
    analogReadResolution(12);
    pinMode(A7, INPUT);

    analogRead(A7);

    uint32_t readingSum = 0;
    for(uint8_t i = 0; i < BATTERY_SAMPLE_COUNT; i++){
        readingSum += analogRead(A7);
        delayMicroseconds(50);
    }

    const float averageReading = (float)readingSum / (float)BATTERY_SAMPLE_COUNT;
    return averageReading * BATTERY_DIVIDER_SCALE * ADC_REFERENCE_VOLTAGE / ADC_MAX_READING;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
char* Loom_Analog::pinNumberToName(int pin){
    // Malloc a name of size 4 
    char* name = (char*)malloc(sizeof(char) * 4);
    snprintf_P(name, 4, PSTR("A%i"), pin - 14);
    return name;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::analogToMV(int analog){
    float analogRes = 4095.0;
    float voltage = (analog * 3.3) / analogRes;
    return voltage * 1000;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::getMV(int pin) {
    for(size_t i = 0; i < pinMappings.size(); i++){
        if(pinMappings[i]->pinNumber == pin){
            return pinMappings[i]->analog_mv;
        }
    }
    return 0.0f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::getAnalog(int pin) {
    for(size_t i = 0; i < pinMappings.size(); i++){
        if(pinMappings[i]->pinNumber == pin){
            return pinMappings[i]->analog;
        }
    }
    return 0.0f;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
