#include "Loom_ADS1115.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ADS1115::Loom_ADS1115(Manager& man, bool useMux, byte address, bool enable_analog, bool enable_diff, adsGain_t gain) : Module("ADS1115"), manInst(&man), i2c_address(address), enableAnalog(enable_analog), enableDiff(enable_diff), adc_gain(gain) {
    module_address = i2c_address;

    if(!useMux)
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
		        volts[i] = ads.computeVolts(analogData[i]);
            }

            if(enableDiff){
                diffData[0] = (int)ads.readADC_Differential_0_1();
                diffData[1] = (int)ads.readADC_Differential_2_3();
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
            json["A0"] = analogData[0];
            json["A1"] = analogData[1];
            json["A2"] = analogData[2];
            json["A3"] = analogData[3];

            json["A0_Volts"] = volts[0];
            json["A1_Volts"] = volts[1];
            json["A2_Volts"] = volts[2];
            json["A3_Volts"] = volts[3];
        }

        if(enableDiff){
            json["Differential_0"] = diffData[0];
            json["Differential_1"] = diffData[1];
        }
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////