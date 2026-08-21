#include "Loom_SEN55.h"
#include "Logger.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SEN55::Loom_SEN55(Manager &man, bool measurePM, bool useMux, bool readNumVals)
    : I2CDevice("SEN55"), manInst(&man), measurePM(measurePM), readNumVals(readNumVals) {

    module_address = 0x69;

    // Register the module with the manager
    if (!useMux)
        manInst->registerModule(this);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::power_up() {
    // Measurement commands are issued by measure(), but after a rail cycle the
    // driver and I2C presence still need to be re-established first. Avoid a
    // device reset here so VOC/NOx state is not discarded when rails stayed on.
    Wire.begin();
    sen5x.begin(Wire);
    delay(1000);
    moduleInitialized = checkDeviceConnection();
    if (!moduleInitialized)
        ERROR(F("SEN55 did not acknowledge after power-up."));
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::initialize() {
    FUNCTION_START;

    /* Initialize wire and start the sensor using the standard I2C interface */
    Wire.begin();
    sen5x.begin(Wire);

    // Attempt to reset the device
    uint16_t error = sen5x.deviceReset();
    if (error) {
        ERRORF("Error %u while resetting SEN55; module will not be initialized.", error);
        moduleInitialized = false;
        FUNCTION_END;
        return;
    } else {
        moduleInitialized = true;
        needsReinit = false;
        LOG(F("Sensor successfully initialized!"));
    }

    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::measure() {
    FUNCTION_START;

    delay(100);
    // Reset the relevent values for the average calcuation of the PM measurements
    resetValuesForMeasure();

    // Turn on pm reading if needed
    if (measurePM) {
        LOG(F("Beginning PM measurement, waiting 2 seconds at each increment for stablizing "
              "mesurement..."));
        uint16_t readErr = sen5x.startMeasurement();
        if (readErr) {
            ERRORF("Failed to start PM measurement (error %u).", readErr);
            FUNCTION_END;
            return;
        }
        float Pm1p0 = 0, Pm2p5 = 0, Pm4p0 = 0, Pm10p0 = 0;
        float numPm0p5 = 0, numPm1p0 = 0, numPm2p5 = 0, numPm4p0 = 0, numPm10p0 = 0;
        float particleSize = 0;
        uint8_t successfulReads = 0;
        for (int i = 0; i < PM_AVERAGE_COUNT; i++) {
            delay(2000);

            bool dataReady = false;
            readErr = sen5x.readDataReady(dataReady);
            if (readErr) {
                ERRORF("Failed to check PM data readiness (error %u).", readErr);
                continue;
            }
            if (!dataReady && i == 0) {
                uint32_t startTime = millis();
                LOG(F("No data available on iteration 0, waiting an additional 5 seconds to see if "
                      "data becomes available"));
                while (!dataReady && (uint32_t)(millis() - startTime) < 5000) {
                    readErr = sen5x.readDataReady(dataReady);
                    if (readErr) {
                        ERRORF("Failed to check PM data readiness (error %u).", readErr);
                        break;
                    }
                    if (!dataReady) {
                        delay(25);
                    }
                }
            }

            if (!dataReady) {
                continue;
            }

            readErr = sen5x.readMeasuredPmValues(Pm1p0, Pm2p5, Pm4p0, Pm10p0, numPm0p5, numPm1p0,
                                                 numPm2p5, numPm4p0, numPm10p0, particleSize);
            if (readErr) {
                ERRORF("Failed to read PM values (error %u).", readErr);
                continue;
            }

            massConcentrationPm1p0 += Pm1p0;
            massConcentrationPm2p5 += Pm2p5;
            massConcentrationPm4p0 += Pm4p0;
            massConcentrationPm10p0 += Pm10p0;
            if (readNumVals) {
                numConcentrationPm0p5 += numPm0p5;
                numConcentrationPm1p0 += numPm1p0;
                numConcentrationPm2p5 += numPm2p5;
                numConcentrationPm4p0 += numPm4p0;
                numConcentrationPm10p0 += numPm10p0;
                typicalParticleSize += particleSize;
            }
            successfulReads++;
            delay(1);
        }

        if (successfulReads > 0) {
            massConcentrationPm1p0 /= successfulReads;
            massConcentrationPm2p5 /= successfulReads;
            massConcentrationPm4p0 /= successfulReads;
            massConcentrationPm10p0 /= successfulReads;

            if (readNumVals) {
                numConcentrationPm0p5 /= successfulReads;
                numConcentrationPm1p0 /= successfulReads;
                numConcentrationPm2p5 /= successfulReads;
                numConcentrationPm4p0 /= successfulReads;
                numConcentrationPm10p0 /= successfulReads;
                typicalParticleSize /= successfulReads;
            }
        }

        else {
            ERROR("Failed to read any data from the sensor, values might be incorrect. If this "
                  "persists, please check for errors...");
        }

        float tmp = 0.0;
        readErr = sen5x.readMeasuredValues(tmp, tmp, tmp, tmp, ambientHumidity, ambientTemperature,
                                           vocIndex, noxIndex);
        if (readErr) {
            ERRORF("Failed to read environment values (error %u).", readErr);
        }

        readErr = sen5x.startMeasurementWithoutPm();
        if (readErr) {
            ERRORF("Failed to switch to non-PM measurement (error %u).", readErr);
        }
        delay(60);
    }

    else {
        LOG("Beginning measurement without PM, waiting 10 seconds for sensor to stabilize...");
        uint16_t readErr = sen5x.startMeasurementWithoutPm();
        if (readErr) {
            ERRORF("Failed to start non-PM measurement (error %u).", readErr);
            FUNCTION_END;
            return;
        }
        delay(10000);

        uint16_t error = 0;
        // Give the sensor time to prepare for measuring
        bool dataReady = false;
        uint32_t startTime = millis();
        LOG(F("Waiting for data to be ready... If not ready in 10 seconds we will stop trying"));
        while (!dataReady && (uint32_t)(millis() - startTime) < 10000) {
            error = sen5x.readDataReady(dataReady);
            if (error) {
                ERRORF("Failed to check if SEN55 data was ready (error %u).", error);
                break;
            }
            if (!dataReady) {
                delay(25);
            }
        }

        // If the data was not ready we don't want to update the sensor values
        if (dataReady) {
            LOG("Device was ready to read a new sample!");
            float tmp = 0.0;
            // Request the measured values form the sensor
            error = sen5x.readMeasuredValues(tmp, tmp, tmp, tmp, ambientHumidity,
                                             ambientTemperature, vocIndex, noxIndex);

            // Check if we had an error reading the sensor values
            if (error) {
                ERRORF("Error %u while reading SEN55 measurement.", error);
                FUNCTION_END;
                return;
            }
        } else {
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
    if (measurePM) {
        json["PM1_0_μg/m³"] = massConcentrationPm1p0;
        json["PM2_5_μg/m³"] = massConcentrationPm2p5;
        json["PM4_0_μg/m³"] = massConcentrationPm4p0;
        json["PM10_0_μg/m³"] = massConcentrationPm10p0;
        if (readNumVals) {
            json["N_PM0_5"] = numConcentrationPm0p5;
            json["N_PM1_0"] = numConcentrationPm1p0;
            json["N_PM2_5"] = numConcentrationPm2p5;
            json["N_PM4_0"] = numConcentrationPm4p0;
            json["N_PM10_0"] = numConcentrationPm10p0;
            json["Typical_Particle_Size"] = typicalParticleSize;
        }
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

    if (moduleInitialized) {
        uint16_t error = sen5x.setTemperatureOffsetSimple(offset);
        if (error)
            ERRORF("Failed to adjust SEN55 sensor offset (error %u).", error);
    }
    FUNCTION_END;
}
//////////////////////////////////////////////////////////////////////////////////////////////////////

//////////////////////////////////////////////////////////////////////////////////////////////////////

void Loom_SEN55::logDeviceStatus() {
    FUNCTION_START;
    uint32_t deviceStatus = 0;

    uint16_t error = sen5x.readDeviceStatus(deviceStatus);

    if (error) {
        ERRORF("Failed to read SEN55 device status (error %u).", error);
        FUNCTION_END;
        return;
    }

    LOGF("Device status: 0x%08lX", static_cast<unsigned long>(deviceStatus));

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
