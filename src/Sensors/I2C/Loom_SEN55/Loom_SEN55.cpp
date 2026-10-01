#include "Loom_SEN55.h"
#include "Logger.h"

namespace {
float sensorValueOrMissing(float value) { return isfinite(value) ? value : -1.0f; }
} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SEN55::Loom_SEN55(Manager &man, bool measurePM, bool useMux, bool readNumVals)
    : I2CDevice("SEN55"), manInst(&man), measurePM(measurePM), readNumVals(readNumVals) {

    module_address = 0x69;

    // Register the module with the manager
    if (!useMux) {
        manInst->registerModule(this);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::power_up() {
    FUNCTION_START(this);
    // Measurement commands are issued by measure(), but after a rail cycle the
    // driver and I2C presence still need to be re-established first. Avoid a
    // device reset here so VOC/NOx state is not discarded when rails stayed on.
    Wire.begin();
    sen5x.begin(Wire);
    delay(1000);
    resetValuesForMeasure(); // A failed rail/bus recovery cannot expose last cycle's data.
    moduleInitialized = checkDeviceConnection();
    needsReinit = !moduleInitialized;
    if (!moduleInitialized) {
        ERROR(F("SEN55 did not acknowledge after power-up."));
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::idle() {
    FUNCTION_START(this);
    // Keep gas learning active where firmware supports switching off just the PM fan.
    // Older firmware stays measuring; a reset/stop would discard useful learning state.
    if (moduleInitialized && supportsDirectModeSwitch) {
        const uint16_t error = sen5x.startMeasurementWithoutPm();
        if (error != 0) {
            ERRORF("SEN55 idle transition failed: %u", error);
        }
        delay(60); // Manufacturer command response margin, matching the normal mode switch.
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::resume() {
    FUNCTION_START(this);
    // measure() explicitly selects PM/non-PM mode and waits for settling before reading.
    // Do not reset algorithms merely because the Manager leaves idle.
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::initialize() {
    FUNCTION_START(this);

    /* Initialize wire and start the sensor using the standard I2C interface */
    Wire.begin();
    sen5x.begin(Wire);

    // Attempt to reset the device
    uint16_t error = sen5x.deviceReset();
    if (error) {
        ERRORF("Error %u while resetting SEN55; module will not be initialized.", error);
        moduleInitialized = false;
        return;
    }
    moduleInitialized = true;
    needsReinit = false;
    uint8_t firmwareMajor = 0, firmwareMinor = 0;
    uint8_t hardwareMajor = 0, hardwareMinor = 0, protocolMajor = 0, protocolMinor = 0;
    bool firmwareDebug = false;
    error = sen5x.getVersion(firmwareMajor, firmwareMinor, firmwareDebug, hardwareMajor,
                             hardwareMinor, protocolMajor, protocolMinor);
    supportsDirectModeSwitch =
        error == 0 && (firmwareMajor > 1 || (firmwareMajor == 1 && firmwareMinor > 0));
    if (error) {
        WARNINGF("SEN55 firmware query failed (error %u); PM will remain running between reads.",
                 error);
    } else {
        LOGF("SEN55 firmware %u.%u; direct PM/gas mode switching: %s", firmwareMajor, firmwareMinor,
             supportsDirectModeSwitch ? "supported" : "unavailable");
        if (measurePM && !supportsDirectModeSwitch) {
            WARNING(F("Older SEN55 firmware: keeping PM running to preserve gas learning; "
                      "power consumption will be higher."));
        }
    }
    LOG(F("Sensor successfully initialized!"));
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::measure() {
    FUNCTION_START(this);
    delay(100);
    resetValuesForMeasure();

    // PM means particulate matter. Each mode has its own startup time and readiness checks.
    // Missing data remains unavailable rather than looking like clean air or an old sample.
    const bool completed = measurePM ? measureWithPm() : measureWithoutPm();
    if (!completed && !checkDeviceConnection()) {
        moduleInitialized = false;
        needsReinit = true; // power_up() retries presence without resetting VOC/NOx learning.
        ERROR(F("SEN55 connection lost; another power-up will retry the connection."));
        return;
    }
    // Status is useful after a failed sample too. Never reset merely because PM or NOx is zero.
    logDeviceStatus();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_SEN55::measureWithPm() {
    FUNCTION_START(this);
    LOG(F("Beginning PM measurement; allowing 30 seconds for fan/statistics startup."));
    uint16_t readErr = sen5x.startMeasurement();
    if (readErr) {
        ERRORF("Failed to start PM measurement (error %u).", readErr);
        return false;
    }
    {
        // Sensirion recommends a settling window before using duty-cycled PM readings.
        // This fixed, local wait exceeds the SAMD watchdog period; restore its configuration
        // immediately afterwards, before any I2C reads or retry loops.
        LoomWatchdogPause watchdogPause;
        delay(PM_WARMUP_MS);
    }
    float Pm1p0 = 0;
    float Pm2p5 = 0;
    float Pm4p0 = 0;
    float Pm10p0 = 0;
    float numPm0p5 = 0;
    float numPm1p0 = 0;
    float numPm2p5 = 0;
    float numPm4p0 = 0;
    float numPm10p0 = 0;
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
        LOOM_FEED_WATCHDOG(); // A readiness transaction completed successfully.
        if (!dataReady && i == 0) {
            const uint32_t startTime = millis();
            LOG(F("No data available on iteration 0, waiting an additional 5 seconds to see if "
                  "data becomes available"));
            while (!dataReady && static_cast<uint32_t>(millis() - startTime) < 5000) {
                readErr = sen5x.readDataReady(dataReady);
                if (readErr) {
                    ERRORF("Failed to check PM data readiness (error %u).", readErr);
                    break;
                }
                LOOM_FEED_WATCHDOG();
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

        const bool validMass = isfinite(Pm1p0) && isfinite(Pm2p5) && isfinite(Pm4p0) &&
                               isfinite(Pm10p0) && Pm1p0 >= 0 && Pm2p5 >= 0 && Pm4p0 >= 0 &&
                               Pm10p0 >= 0;
        const bool validNumber =
            !readNumVals ||
            (isfinite(numPm0p5) && isfinite(numPm1p0) && isfinite(numPm2p5) && isfinite(numPm4p0) &&
             isfinite(numPm10p0) && isfinite(particleSize) && numPm0p5 >= 0 && numPm1p0 >= 0 &&
             numPm2p5 >= 0 && numPm4p0 >= 0 && numPm10p0 >= 0 && particleSize >= 0);
        if (!validMass || !validNumber) {
            WARNING(F("SEN55 PM sample is unavailable; excluding it from the average."));
            continue;
        }

        // Only the first valid sample clears the unavailable marker. An entirely failed
        // window therefore cannot accidentally package an accumulator's initial zero.
        if (successfulReads == 0) {
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
        ERROR(F("No valid SEN55 PM samples; recording unavailable values, not zero."));
    }

    float tmp = 0.0;
    readErr = sen5x.readMeasuredValues(tmp, tmp, tmp, tmp, ambientHumidity, ambientTemperature,
                                       vocIndex, noxIndex);
    if (readErr) {
        ERRORF("Failed to read environment values (error %u).", readErr);
    }

    const bool environmentRead = readErr == 0;
    if (supportsDirectModeSwitch) {
        // Firmware > 1.0 supports this direct transition. Stop/reset would discard or
        // interrupt gas learning; older/unknown firmware is left in continuous PM mode.
        readErr = sen5x.startMeasurementWithoutPm();
        if (readErr) {
            ERRORF("Failed to switch to non-PM measurement (error %u).", readErr);
            return false;
        }
    }
    delay(60);
    return successfulReads > 0 && environmentRead;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
bool Loom_SEN55::measureWithoutPm() {
    FUNCTION_START(this);
    LOG("Beginning measurement without PM, waiting 10 seconds for sensor to stabilize...");
    uint16_t readErr = sen5x.startMeasurementWithoutPm();
    if (readErr) {
        ERRORF("Failed to start non-PM measurement (error %u).", readErr);
        return false;
    }
    {
        LoomWatchdogPause watchdogPause;
        delay(10000); // Fixed stabilization wait; the watchdog is restored before polling.
    }

    uint16_t error = 0;
    // Give the sensor time to prepare for measuring
    bool dataReady = false;
    const uint32_t startTime = millis();
    LOG(F("Waiting for data to be ready... If not ready in 10 seconds we will stop trying"));
    while (!dataReady && static_cast<uint32_t>(millis() - startTime) < 10000) {
        error = sen5x.readDataReady(dataReady);
        if (error) {
            ERRORF("Failed to check if SEN55 data was ready (error %u).", error);
            break;
        }
        LOOM_FEED_WATCHDOG();
        if (!dataReady) {
            delay(25);
        }
    }

    // This cycle began with unavailable markers, so a timeout cannot reuse the last reading.
    if (dataReady) {
        LOG("Device was ready to read a new sample!");
        float tmp = 0.0;
        // Request the measured values form the sensor
        error = sen5x.readMeasuredValues(tmp, tmp, tmp, tmp, ambientHumidity, ambientTemperature,
                                         vocIndex, noxIndex);

        // Check if we had an error reading the sensor values
        if (error) {
            ERRORF("Error %u while reading SEN55 measurement.", error);
            return false;
        }
    } else {
        ERROR("No new data was ready within the given time period.");
        return false;
    }
    return true;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::package() {
    FUNCTION_START(this);
    JsonObject json = manInst->get_data_object(getModuleName());

    // Only include the PM measurements if we are actually measuring PM
    if (measurePM) {
        json["PM1_0_μg/m³"] = sensorValueOrMissing(massConcentrationPm1p0);
        json["PM2_5_μg/m³"] = sensorValueOrMissing(massConcentrationPm2p5);
        json["PM4_0_μg/m³"] = sensorValueOrMissing(massConcentrationPm4p0);
        json["PM10_0_μg/m³"] = sensorValueOrMissing(massConcentrationPm10p0);
        if (readNumVals) {
            json["N_PM0_5"] = sensorValueOrMissing(numConcentrationPm0p5);
            json["N_PM1_0"] = sensorValueOrMissing(numConcentrationPm1p0);
            json["N_PM2_5"] = sensorValueOrMissing(numConcentrationPm2p5);
            json["N_PM4_0"] = sensorValueOrMissing(numConcentrationPm4p0);
            json["N_PM10_0"] = sensorValueOrMissing(numConcentrationPm10p0);
            json["Typical_Particle_Size"] = sensorValueOrMissing(typicalParticleSize);
        }
    }
    json["AmbientHumidity_%RH"] = sensorValueOrMissing(ambientHumidity);
    json["AmbientTemperature_°C"] = sensorValueOrMissing(ambientTemperature);
    json["VocIndex_0-500"] = sensorValueOrMissing(vocIndex);
    json["NoxIndex_1-500"] = sensorValueOrMissing(noxIndex);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::adjustTempOffset(float offset) {
    FUNCTION_START(this);

    if (moduleInitialized) {
        uint16_t error = sen5x.setTemperatureOffsetSimple(offset);
        if (error) {
            ERRORF("Failed to adjust SEN55 sensor offset (error %u).", error);
        }
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::logDeviceStatus() {
    FUNCTION_START(this);
    uint32_t deviceStatus = 0;

    uint16_t error = sen5x.readDeviceStatus(deviceStatus);

    if (error) {
        ERRORF("Failed to read SEN55 device status (error %u).", error);
        return;
    }

    LOGF("Device status: 0x%08lX", static_cast<unsigned long>(deviceStatus));
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN55::resetValuesForMeasure() {
    FUNCTION_START(this);
    massConcentrationPm1p0 = NAN;
    massConcentrationPm2p5 = NAN;
    massConcentrationPm4p0 = NAN;
    massConcentrationPm10p0 = NAN;
    numConcentrationPm0p5 = NAN;
    numConcentrationPm1p0 = NAN;
    numConcentrationPm2p5 = NAN;
    numConcentrationPm4p0 = NAN;
    numConcentrationPm10p0 = NAN;
    typicalParticleSize = NAN;
    ambientHumidity = NAN;
    ambientTemperature = NAN;
    vocIndex = NAN;
    noxIndex = NAN;
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
