#include "Loom_Analog.h"


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Analog::measure(){

    // Read the data from the given analog pin
    for(int i = 0; i < pinMappings.size(); i++){

        /* If we are measuring the Vbat pin we want a little different behavior */
        if(pinMappings[i]->pinNumber == A7){
            pinMappings[i]->analog = getBatteryVoltage();
            pinMappings[i]->analog_mv = getBatteryVoltage() * 1000;
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
    for(int i = 0; i < pinMappings.size(); i ++){
        memset(output, '\0', 10);
        json[pinMappings[i]->name] = pinMappings[i]->analog;

        /* Append MV to the name to differentiate between normal analog and the millivolt representation */
        strncat(output, pinMappings[i]->name, 10);
        strncat(output, "_MV", 10);
        json[output] = pinMappings[i]->analog_mv;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::getBatteryVoltage(){
    float pin_reading = analogRead(A7);
    pin_reading *= 2;
    pin_reading *= 3.3;
    pin_reading /= 4096;
    return pin_reading;
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
    for(int i = 0; i < pinMappings.size(); i++){
        if(pinMappings[i]->pinNumber == pin){
            return pinMappings[i]->analog_mv;
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::getAnalog(int pin) {
    for(int i = 0; i < pinMappings.size(); i++){
        if(pinMappings[i]->pinNumber == pin){
            return pinMappings[i]->analog;
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////