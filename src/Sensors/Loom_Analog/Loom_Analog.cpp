#include "Loom_Analog.h"


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Analog::measure(){
    char name[4];

    // Clear the map before adding new data
    pinToData.clear();

    // Read the data from the given analog pin
    for(int i = 0; i < analogPins.size(); i++){
        int analogData = analogRead(analogPins[i]);
        pin_number_to_name(analogPins[i], name);

        // Needs to be cast to a const char* so that the name stays in memory 
        pinToData.insert(std::make_pair((const char*)name, std::make_pair(analogData, analogToMV(analogData))));
    }

    // Pull the battery voltage and add it to the top of 
    pinToData.insert(std::make_pair("Vbat", std::make_pair(get_battery_voltage(), get_battery_voltage() * 1000)));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Analog::package(){
    char output[21];
    JsonObject json = manInst->get_data_object(getModuleName());
    for ( const auto &myPair : pinToData ) {
        memset(output, '\0', 20);
        json[myPair.first] = pinToData[myPair.first].first;
        strncat(output, myPair.first, 20);
        strncat(output, "_MV", 20);
        json[(const char*)output] = pinToData[myPair.first].second;
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::get_battery_voltage(){
    float pin_reading = analogRead(A7);
    pin_reading *= 2;
    pin_reading *= 3.3;
    pin_reading /= 4096;
    return pin_reading;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Analog::pin_number_to_name(int pin, char name[4]){
    memset(name, '\0', 4);
    snprintf_P(name, 4, PSTR("A%i"), pin - 14);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
float Loom_Analog::analogToMV(int analog){
    float analogRes = 4095.0;
    float voltage = (analog * 3.3) / analogRes;
    return voltage * 1000;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////
