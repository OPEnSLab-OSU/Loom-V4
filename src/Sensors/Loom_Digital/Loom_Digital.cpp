#include "Loom_Digital.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Digital::measure(){
    
    // Clear collected data
    pinToData.clear();

    // Read the data from the given analog pin
    for(int i = 0; i < digitalPins.size(); i++){
        pinToData.insert(std::pair<int, int>(digitalPins[i], digitalRead(digitalPins[i])));
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_Digital::package(){
    JsonObject json = manInst->get_data_object(getModuleName());
    for ( const auto &myPair : pinToData ) {
       json[String(myPair.first)] = pinToData[myPair.first];
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////