#include "Loom_SEN55.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SEN55::Loom_SEN55(
                        Manager& man,
                        bool measurePM,
                        bool useMux,
                        bool readNumVals
                    ) : I2CDevice("SEN55"), manInst(&man), measurePM(measurePM), readNumVals(readNumVals){

                        // Register the module with the manager
                        if(!useMux)
                            manInst->registerModule(this);
                    }
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::initialize() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char errorMessage[OUTPUT_SIZE];

    /* Initialize wire and start the sensor using the standard I2C interface */
    Wire.begin();
    sen5x.begin(Wire);

    // Attempt to reset the device
    uint16_t error = sen5x.deviceReset();
    if(error){
        /* Stringify the errro and log the error */
        errorToString(error, errorMessage, OUTPUT_SIZE);
        snprintf(output, OUTPUT_SIZE, "Error occurred while attempting to reset device: %s, module will not be initialized!", errorMessage);
        ERROR(output);
        moduleInitialized = false;
        FUNCTION_END;
        return;
    }
    else{
        LOG("Sensor successfully initialized!");
    }

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::measure() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char sensorError[OUTPUT_SIZE];

    delay(100);
    // Reset the relevent values for the average calcuation of the PM measurements
    resetValuesForMeasure();

    // Turn on pm reading if needed
    if(measurePM){
        LOG(F("Beginning PM measurement, waiting 2 seconds at each increment for stablizing mesurement..."));
        uint16_t readErr = sen5x.startMeasurement();
        float Pm1p0 = 0, Pm2p5 = 0, Pm4p0 = 0, Pm10p0 = 0;
        float numPm0p5 = 0, numPm1p0 = 0, numPm2p5 = 0, numPm4p0 = 0, numPm10p0 = 0;
        float particleSize = 0;
        uint8_t failedReads = 0;
        for(int i = 0; i < PM_AVERAGE_COUNT; i++){
            delay(2000);

            bool dataReady = false;
            sen5x.readDataReady(dataReady);
            if(!dataReady && i == 0){
                uint16_t startTime = millis();
                LOG(F("No data available on iteration 0, waiting an additional 5 seconds to see if data becomes available"));
                while(!dataReady && millis() < startTime + 5000){
                    sen5x.readDataReady(dataReady);
                }
            }

            if(dataReady)
                sen5x.readMeasuredPmValues(Pm1p0, Pm2p5, Pm4p0, Pm10p0, numPm0p5, numPm1p0,
                                        numPm2p5, numPm4p0, numPm10p0, particleSize);
            else{
                failedReads++;
            }

            massConcentrationPm1p0 += Pm1p0;
            massConcentrationPm2p5 += Pm2p5;
            massConcentrationPm4p0 += Pm4p0;
            massConcentrationPm10p0 += Pm10p0;
            if(readNumVals){
                numConcentrationPm0p5 += numPm0p5;
                numConcentrationPm1p0 += numPm1p0;
                numConcentrationPm2p5 += numPm2p5;
                numConcentrationPm4p0 += numPm4p0;
                numConcentrationPm10p0 += numPm10p0;
                typicalParticleSize += particleSize;
            }
            delay(1);
        }

        if(failedReads < PM_AVERAGE_COUNT){
            // Calculate the average of the values (excluding failed reads)
            massConcentrationPm1p0 /= (PM_AVERAGE_COUNT - failedReads);
            massConcentrationPm2p5 /= (PM_AVERAGE_COUNT - failedReads);
            massConcentrationPm4p0 /= (PM_AVERAGE_COUNT - failedReads);
            massConcentrationPm10p0 /= (PM_AVERAGE_COUNT - failedReads);;

            if(readNumVals){
                numConcentrationPm0p5 /= (PM_AVERAGE_COUNT - failedReads);;
                numConcentrationPm1p0 /= (PM_AVERAGE_COUNT - failedReads);;
                numConcentrationPm2p5 /= (PM_AVERAGE_COUNT - failedReads);;
                numConcentrationPm4p0 /= (PM_AVERAGE_COUNT - failedReads);;
                numConcentrationPm10p0 /= (PM_AVERAGE_COUNT - failedReads);;
                typicalParticleSize /= (PM_AVERAGE_COUNT - failedReads);;
            }
        }

        else{
            ERROR("Failed to read any data from the sensor, values might be incorrect. If this persists, please check for errors...");
        }

        float tmp = 0.0;
        readErr = sen5x.readMeasuredValues(
                                            tmp, tmp, tmp, tmp,
                                            ambientHumidity, ambientTemperature, vocIndex,
                                            noxIndex
                                        );

        readErr = sen5x.startMeasurementWithoutPm();
        delay(60);
    }

    else {
        LOG("Beginning measurement without PM, waiting 10 seconds for sensor to stabilize...");
        uint16_t readErr = sen5x.startMeasurementWithoutPm();
        delay(10000);

        uint16_t error = 0;
        // Give the sensor time to prepare for measuring
        bool dataReady = false;
        uint16_t startTime = millis();
        LOG(F("Waiting for data to be ready... If not ready in 10 seconds we will stop trying"));
        while(!dataReady && millis() < startTime + 10000){
            error = sen5x.readDataReady(dataReady);
            if(error){
                errorToString(error, sensorError, OUTPUT_SIZE);
                snprintf(output, OUTPUT_SIZE, "Failed to check if data was ready to be read: %s", sensorError);
                ERROR(output);
            }
        }

        // If the data was not ready we don't want to update the sensor values
        if(dataReady){
            LOG("Device was ready to read a new sample!");
            float tmp = 0.0;
            // Request the measured values form the sensor
            error = sen5x.readMeasuredValues(
                                                tmp, tmp, tmp, tmp,
                                                ambientHumidity, ambientTemperature, vocIndex,
                                                noxIndex
                                            );

            // Check if we had an error reading the sensor values
            if(error){
                errorToString(error, sensorError, OUTPUT_SIZE);
                snprintf(output, OUTPUT_SIZE, "Error occurred when reading measurement: %s", sensorError);
                ERROR(output);
                FUNCTION_END;
                return;
            }
        }else{
            ERROR("No new data was ready within the given time period.");
        }
    }

    /* TODO: Implement this once we know the raw integration works.
    // Get the current connection status
    bool connectionStatus = checkDeviceConnection();

    // If we are connected and we need to reinit
    if(connectionStatus && needsReinit){
        initialize();
        needsReinit = false;
    }

    // If we are not connected
    else if(!connectionStatus){
        ERROR(F("No acknowledge received from the device"));
        FUNCTION_END;
        return;
    }*/

    /* Attempt to initiate a measurement with the sensor */
    
    // Log Device status after measuring to check for errors
    logDeviceStatus();

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::package() {
    FUNCTION_START;
    JsonObject json = manInst->get_data_object(getModuleName());

    // Only include the PM measurements if we are actually measuring PM
    if(measurePM){
        json["PM1_0_μg/m³"] = massConcentrationPm1p0; 
        json["PM2_5_μg/m³"] = massConcentrationPm2p5;
        json["PM4_0_μg/m³"] = massConcentrationPm4p0;
        json["PM10_0_μg/m³"] = massConcentrationPm10p0;
    }
    json["AmbientHumidity_%RH"] = (isnan(ambientHumidity) ? -1 : ambientHumidity);
    json["AmbientTemperature_°C"] = (isnan(ambientTemperature) ? -1 : ambientTemperature);
    json["VocIndex_0-500"] = (isnan(vocIndex) ? -1 : vocIndex);
    json["NoxIndex_1-500"] = (isnan(noxIndex) ? -1 : noxIndex);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::adjustTempOffset(float offset) {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char sensorError[OUTPUT_SIZE];

    if(moduleInitialized){
        uint16_t error = sen5x.setTemperatureOffsetSimple(offset);
        if(error){
            errorToString(error, sensorError, OUTPUT_SIZE);
            snprintf(output, OUTPUT_SIZE, "Failed to adjust sensor offset: %s", sensorError);
            ERROR(output);
        }
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////


void Loom_SEN55::logDeviceStatus() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char sensorError[OUTPUT_SIZE];

    

    uint32_t deviceStatus;

    uint16_t error = sen5x.readDeviceStatus(deviceStatus);

    std::bitset<32> bits(deviceStatus);

    std::string bitString = bits.to_string();

    snprintf(output, OUTPUT_SIZE, "Device Status: %s", bitString.c_str());
    LOG(output);

    if(error){
            errorToString(error, sensorError, OUTPUT_SIZE);
            snprintf(output, OUTPUT_SIZE, "Error occurred while logging device status: %s", sensorError);
            ERROR(output);
        }

    FUNCTION_END;

}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::resetValuesForMeasure() {
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
    typicalParticleSize = 0;
    FUNCTION_END;
    return;
}