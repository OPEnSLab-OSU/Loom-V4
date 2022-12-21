#include "Loom_ADS1115.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_ADS1115::Loom_ADS1115(Manager& man, byte address, bool useMux,  bool enable_analog, bool enable_diff, adsGain_t gain) : I2CDevice("ADS1115"), manInst(&man), i2c_address(address), enableAnalog(enable_analog), enableDiff(enable_diff), adc_gain(gain) {
    module_address = i2c_address;

    if(!useMux)
        manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::initialize(){

    if(!ads.begin(i2c_address)){
        printModuleName(); Serial.println("Failed to initialize ADS1115 interface! Data may be invalid");
        moduleInitialized = false;
    }
    else{
        printModuleName(); Serial.println("Successfully initialized sensor!");
    }

    // Set the gain of the ADC
    ads.setGain(adc_gain);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::measure(){
    if(moduleInitialized){
        // Get the current connection status
        bool connectionStatus = checkDeviceConnection();

        // If we are connected and we need to reinit
        if(connectionStatus && needsReinit){
            initialize();
            needsReinit = false;
        }

        // If we are not connected
        else if(!connectionStatus){
            printModuleName(); Serial.println("No acknowledge received from the device");
            return;
        }

    
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
    if(moduleInitialized){
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


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_ADS1115::power_up(){

    // Reinitialize the gain
    if(moduleInitialized){
        ads.setGain(adc_gain);
    }
}
//////////////////////////////////////////////////////////////////////////////////////////////////////