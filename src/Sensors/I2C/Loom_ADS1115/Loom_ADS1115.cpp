#include "Loom_ADS1115.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ADS1115::Loom_ADS1115(Manager& man, byte address, bool enable_analog ,bool enable_diff, adsGain_t gain) : Module("ADS1115"), manInst(&man), i2c_address(address), enableAnalog(enable_analog), enableDiff(enable_diff), adc_gain(gain) {
    manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::initialize(){

    // Set the gain of the ADC
    ads.setGain(adc_gain);

    if(!ads.begin()){
        printModuleName(); Serial.println("Failed to initialize ADS1115 interface! Data may be invalid");
        initialized = false;
    }
    else{
        printModuleName(); Serial.println("Successfully initialized sensor!");
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::measure(){
    if(initialized){
        if(enableAnalog){
            for(int i = 0; i < 4; i++){
                analogData[i] = ads.readADC_SingleEnded(i);
            }

            if(enableDiff){
                diffData[0] = ads.readADC_Differential_0_1();
                diffData[1] = ads.readADC_Differential_2_3();
            }
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::package(){
    if(initialized){
        JsonObject json = manInst->get_data_object(getModuleName());
        if(enableAnalog){
            json["Analog 0"] = analogData[0];
            json["Analog 1"] = analogData[1];
            json["Analog 2"] = analogData[2];
            json["Analog 3"] = analogData[3];
        }

        if(enableDiff){
            json["Differential 0"] = diffData[0];
            json["Differential 1"] = diffData[1];
        }

        // If we have custom calculations we want to run
        if(customCalculations.size() > 0){

            // For each pair in the map we want to set the key name to the supplied name and then run the user supplied function
            for(const auto &mapPair : customCalculations){
                Primative prim;

                //Call the supplied function
                (*mapPair.second)(prim, analogData, diffData);

                json[mapPair.first] = prim.getData();
               
            }
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::addCustomCalculation(calcFunction func, String keyName){
    // Add our function to the list
    std::pair<String, calcFunction> tempPair = std::make_pair(keyName, func);
    customCalculations.insert(tempPair);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


