#include "Loom_SEN55.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SEN55::Loom_SEN55(
                        Manager& man,
                        bool measurePM,
                        bool useMux
                    ) : I2CDevice("SEN55"), manInst(&man), measurePM(measurePM){

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

    /* Determine if we want to measure with the particulate matter readings or not */
    if(measurePM){
        error = sen5x.startMeasurement();
    }
    else{
        error = sen5x.startMeasurementWithoutPm();
    }

    if(error){
        errorToString(error, errorMessage, OUTPUT_SIZE);
        snprintf(output, OUTPUT_SIZE, "Error occurred when attempting to collect measurement: %s", errorMessage);
        ERROR(output);
        FUNCTION_END;
        return;
    }

    // Wait for required one second as described in the readMeasuredValues() documentation
    LOG("Waiting 10 seconds seconds so that we can warm the sensor up");
    delay(10000);
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::measure() {
    FUNCTION_START;
    char output[OUTPUT_SIZE];
    char sensorError[OUTPUT_SIZE];

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

    uint16_t error = 0;

    // Give the sensor time to prepare for measuring
    bool dataReady = false;
    uint16_t startTime = millis();
    LOG("Waiting for data to be ready... If not ready in 10 seconds we will stop trying");
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
        // Request the measured values form the sensor
        error = sen5x.readMeasuredValues(
                                            massConcentrationPm1p0, massConcentrationPm2p5, massConcentrationPm4p0,
                                            massConcentrationPm10p0, ambientHumidity, ambientTemperature, vocIndex,
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

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::package() {
    FUNCTION_START;
    JsonObject json = manInst->get_data_object(getModuleName());

    // Only include the PM measurements if we are actually measuring PM
    if(measurePM){
        json["PM1_0"] = massConcentrationPm10p0;
        json["PM2_5"] = massConcentrationPm2p5;
        json["PM4_0"] = massConcentrationPm4p0;
        json["PM10_0"] = massConcentrationPm10p0;
    }
    json["AmbientHumidity"] = (isnan(ambientHumidity) ? -1 : ambientHumidity);
    json["AmbientTemperature"] = (isnan(ambientTemperature) ? -1 : ambientTemperature);
    json["VocIndex"] = (isnan(vocIndex) ? -1 : vocIndex);
    json["NoxIndex"] = (isnan(noxIndex) ? -1 : noxIndex);
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