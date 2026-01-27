#include "Loom_SEN66.h"
#include "Logger.h"


//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SEN66::Loom_SEN66(
                        Manager& man,
                        bool measurePM,
                        bool useMux,
                        bool readNumVals
                    ) : I2CDevice("SEN66"), manInst(&man), measurePM(measurePM), readNumVals(readNumVals){
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::initialize() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char errorMessage[OUTPUT_SIZE];


    Wire.begin();
    sen66.begin(Wire, SEN66_I2C_ADDRESS);


    // Attempt to reset the device
    uint16_t error = sen66.deviceReset();
   
    if(error){
        snprintf(errorMessage, OUTPUT_SIZE, "Error code: %u", error);
        snprintf(output, OUTPUT_SIZE, "Reset Failed: %s", errorMessage);
        ERROR(output);
        moduleInitialized = false;
        FUNCTION_END;
        return;
    } else {
        LOG("Sensor successfully reset!");
    }
   
    LOG("Resetting SEN66, waiting 1.2s...");
    delay(1200);


   
    // Start Continuous Measurement
    error = sen66.startContinuousMeasurement();
    if (error) {
        snprintf(output, OUTPUT_SIZE, "Error starting measurement: %u", error);
        ERROR(output);
    } else {
        LOG("SEN66 Started. Waiting 5s for fan spin-up...");
        delay(5000); // Initial spin-up delay
    }


    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::measure() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];


    // Reset internal accumulators to 0
    resetValuesForMeasure();


    // Temporary variables
    float tmpPm1p0, tmpPm2p5, tmpPm4p0, tmpPm10p0;
    float tmpHum, tmpTemp, tmpVoc, tmpNox;
    uint16_t tmpCo2;
    float tmpNumPm0p5, tmpNumPm1p0, tmpNumPm2p5, tmpNumPm4p0, tmpNumPm10p0;


    // Accumulators
    float accPm1p0 = 0, accPm2p5 = 0, accPm4p0 = 0, accPm10p0 = 0;
    float accHum = 0, accTemp = 0, accVoc = 0, accNox = 0;
    long accCo2 = 0;
    float accNumPm0p5 = 0, accNumPm1p0 = 0, accNumPm2p5 = 0, accNumPm4p0 = 0, accNumPm10p0 = 0;


    int validSamples = 0;
    uint16_t error = 0;
   
    LOG(F("Reading SEN66 data..."));


    // We try to read PM_AVERAGE_COUNT samples
    for(int i = 0; i < PM_AVERAGE_COUNT; i++){
       
        // Wait 1 second for next data point (Sensor updates @ 1Hz)
        delay(1000);


        uint8_t padding;
        bool dataReady = false;
        sen66.getDataReady(padding, dataReady); // Check if new data is ready


        if(dataReady){
            // Read Values
            error = sen66.readMeasuredValues(tmpPm1p0, tmpPm2p5, tmpPm4p0, tmpPm10p0,
                                             tmpHum, tmpTemp, tmpVoc, tmpNox, tmpCo2);
           
            // Filter out Error/NotReady values (High PM or 0xFFFF CO2)
            if(error == 0 && tmpPm2p5 < 6000.0 && tmpCo2 < 60000){
               
                accPm1p0 += tmpPm1p0;
                accPm2p5 += tmpPm2p5;
                accPm4p0 += tmpPm4p0;
                accPm10p0 += tmpPm10p0;
                accHum += tmpHum;
                accTemp += tmpTemp;
                accVoc += tmpVoc;
                accNox += tmpNox;
                accCo2 += tmpCo2;


                if(readNumVals){
                        error = sen66.readNumberConcentrationValues(tmpNumPm0p5, tmpNumPm1p0, tmpNumPm2p5,
                                                                    tmpNumPm4p0, tmpNumPm10p0);
                        if(error == 0){
                            accNumPm0p5 += tmpNumPm0p5;
                            accNumPm1p0 += tmpNumPm1p0;
                            accNumPm2p5 += tmpNumPm2p5;
                            accNumPm4p0 += tmpNumPm4p0;
                            accNumPm10p0 += tmpNumPm10p0;
                        }
                }
                validSamples++;
            } else {
                 // Debug print
                 if(error) {
                     snprintf(output, OUTPUT_SIZE, "Read Error: %u", error);
                     ERROR(output);
                 } else {
                     snprintf(output, OUTPUT_SIZE, "Invalid Data Skipped (PM2.5: %.2f, CO2: %u)", tmpPm2p5, tmpCo2);
                     ERROR(output);
                 }
            }
        }
    }


    // Calculate Averages
    if(validSamples > 0){
        massConcentrationPm1p0 = accPm1p0 / validSamples;
        massConcentrationPm2p5 = accPm2p5 / validSamples;
        massConcentrationPm4p0 = accPm4p0 / validSamples;
        massConcentrationPm10p0 = accPm10p0 / validSamples;
        ambientHumidity = accHum / validSamples;
        ambientTemperature = accTemp / validSamples;
        vocIndex = accVoc / validSamples;
        noxIndex = accNox / validSamples;
        co2 = (uint16_t)(accCo2 / validSamples);


        if(readNumVals){
            numConcentrationPm0p5 = accNumPm0p5 / validSamples;
            numConcentrationPm1p0 = accNumPm1p0 / validSamples;
            numConcentrationPm2p5 = accNumPm2p5 / validSamples;
            numConcentrationPm4p0 = accNumPm4p0 / validSamples;
            numConcentrationPm10p0 = accNumPm10p0 / validSamples;
        }
    } else {
        ERROR("No valid samples collected. Outputting 0s.");
    }
    logDeviceStatus();


    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::package() {
    FUNCTION_START;
    JsonObject json = manInst->get_data_object(getModuleName());


    if(measurePM){
        json["PM1_0"] = massConcentrationPm1p0;
        json["PM2_5"] = massConcentrationPm2p5;
        json["PM4_0"] = massConcentrationPm4p0;
        json["PM10_0"] = massConcentrationPm10p0;


        if(readNumVals){
            json["N_PM0_5"] = numConcentrationPm0p5;
            json["N_PM1_0"] = numConcentrationPm1p0;
            json["N_PM2_5"] = numConcentrationPm2p5;
            json["N_PM4_0"] = numConcentrationPm4p0;
            json["N_PM10_0"] = numConcentrationPm10p0;
        }
    }
   
    json["AmbientHumidity"] = (isnan(ambientHumidity) ? -1 : ambientHumidity);
    json["AmbientTemperature"] = (isnan(ambientTemperature) ? -1 : ambientTemperature);
    json["VocIndex"] = (isnan(vocIndex) ? -1 : vocIndex);
    json["NoxIndex"] = (isnan(noxIndex) ? -1 : noxIndex);
    json["CO2"] = co2;


    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::adjustTempOffset(int16_t offset, int16_t slope, uint16_t timeConstant) {
    FUNCTION_START;
    char output[OUTPUT_SIZE];


    if(moduleInitialized){
        uint16_t error = sen66.setTemperatureOffsetParameters(offset, slope, timeConstant, 0);
        if(error){
            snprintf(output, OUTPUT_SIZE, "Failed to adjust sensor offset. Error: %u", error);
            ERROR(output);
        }
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::logDeviceStatus() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];


    SEN66DeviceStatus deviceStatus;
    uint16_t error = sen66.readDeviceStatus(deviceStatus);


    if(!error){
        std::bitset<32> bits(deviceStatus.value);
        std::string bitString = bits.to_string();
        snprintf(output, OUTPUT_SIZE, "Device Status: %s", bitString.c_str());
        LOG(output);
    } else {
        snprintf(output, OUTPUT_SIZE, "Error logging device status: %u", error);
        ERROR(output);
    }


    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////


//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::resetValuesForMeasure() {
    FUNCTION_START;
    massConcentrationPm1p0 = 0;
    massConcentrationPm2p5 = 0;
    massConcentrationPm4p0 = 0;
    massConcentrationPm10p0 = 0;
    numConcentrationPm0p5 = 0;
    numConcentrationPm1p0 = 0;
    numConcentrationPm2p5 = 0;
    numConcentrationPm4p0 = 0;
    numConcentrationPm10p0 = 0;
    ambientHumidity = 0;
    ambientTemperature = 0;
    vocIndex = 0;
    noxIndex = 0;
    co2 = 0;
    FUNCTION_END;
}

