#include "Loom_SEN66.h"
#include "Logger.h"
#include "Loom_Manager.h"
#include "Utilities/Loom_SensorAverage.h"
#include "Utilities/Loom_Watchdog.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
Loom_SEN66::Loom_SEN66(Manager &man, bool measurePM, bool useMux, bool readNumVals)
    : I2CDevice("SEN66"), manInst(&man), measurePM(measurePM), readNumVals(readNumVals) {
    module_address = SEN66_I2C_ADDRESS;

    if (!useMux) {
        manInst->registerModule(this);
    }
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::initialize() {
    FUNCTION_START(this);
    Wire.begin();
    sen66.begin(Wire, SEN66_I2C_ADDRESS);
    delay(100); // Datasheet: at least 100 ms after power-on before I2C commands.

    // A retained sensor rail lets the gas algorithms keep learning while the Feather sleeps.
    // A false ready flag can also mean "just read". Check again after the slowest 1.03 s update
    // before deciding the sensor needs a restart. An I2C ACK alone does not prove it is running.
    uint8_t padding = 0;
    bool dataReady = false;
    int16_t error = sen66.getDataReady(padding, dataReady);
    if (error != 0 || !dataReady) {
        delay(1050);
        error = sen66.getDataReady(padding, dataReady);
    }
    if (error != 0 || padding != 0) {
        ERRORF("SEN66 readiness query failed: %d; leaving sensor state untouched.", error);
        moduleInitialized = false;
        needsReinit = true;
        return;
    }
    LOOM_FEED_WATCHDOG(); // A CRC-checked readiness reply confirms communication progress.
    if (dataReady) {
        if (!measurementStarted) {
            // MCU restart with the sensor already powered: its measurement age is unknown.
            measurementStartedAt = millis();
            pmSettled = false;
        }
        measurementStarted = true;
        moduleInitialized = true;
        needsReinit = false;
        LOG(F("SEN66 is already measuring; keeping its running algorithms."));
        return;
    }

    measurementStarted = false;
    pmSettled = false;
    // Reset is valid only in idle mode. Stop first, including when recovering a stalled sensor.
    // The installed driver waits 1000 ms; current Sensirion docs require 1400 ms, so add 400 ms.
    error = sen66.stopMeasurement();
    if (error == 0) {
        delay(400);
        error = sen66.deviceReset(); // The manufacturer driver already waits the required 1200 ms.
    }
    if (error == 0) {
        LOOM_FEED_WATCHDOG(); // Stop/reset completed; retain protection during the next command.
        error = sen66.startContinuousMeasurement(); // Driver includes the command's 50 ms wait.
    }
    if (error != 0) {
        ERRORF("SEN66 startup failed: %d", error);
        moduleInitialized = false;
        needsReinit = true;
        return;
    }
    measurementStartedAt = millis();
    measurementStarted = true;
    moduleInitialized = true;
    needsReinit = false;
    LOG(F("SEN66 started; PM settling will finish before its first sample window."));
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::idle() {
    FUNCTION_START(this);
    if (moduleInitialized && measurementStarted && !retainedIdle) {
        const int16_t error = sen66.stopMeasurement();
        if (error == 0) {
            delay(400); // Installed driver waits 1000 ms; datasheet stop time is 1400 ms.
            retainedIdle = true;
            measurementStarted = false;
            pmSettled = false;
        } else {
            ERRORF("SEN66 idle stop failed: %d", error);
        }
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::resume() {
    FUNCTION_START(this);
    if (retainedIdle) {
        const int16_t error = sen66.startContinuousMeasurement();
        measurementStarted = error == 0;
        needsReinit = error != 0;
        measurementStartedAt = millis();
        pmSettled = false;
        retainedIdle = false;
        if (error != 0) {
            ERRORF("SEN66 resume failed: %d", error);
        }
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::power_up() {
    FUNCTION_START(this);
    retainedIdle = false;
    // Detect both supported sleep configurations: power removed or sensor rail retained.
    initialize();
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::measure() {
    FUNCTION_START(this);
    resetValuesForMeasure();
    if (retainedIdle) {
        // A mux port may have failed selection during Manager::resume(). Retry once now that
        // measurement was requested on this port, without resetting the sensor's algorithms.
        resume();
    }
    if (needsReinit) {
        initialize(); // Also recover in sketches that measure repeatedly without Hypnos sleep.
    }
    if (!moduleInitialized || !measurementStarted) {
        return;
    }

    if (measurePM && !pmSettled) {
        const uint32_t elapsed = static_cast<uint32_t>(millis() - measurementStartedAt);
        if (elapsed < PM_WARMUP_MS) {
            LOG(F("Waiting for SEN66 particulate-matter startup (30 seconds total)..."));
            // Pause only this known, bounded delay. I2C operations retain watchdog protection.
            LoomWatchdogPause pause;
            delay(PM_WARMUP_MS - elapsed);
        }
        pmSettled = true;
        // PM readiness is not VOC/NOx convergence: those algorithms need hours of operation.
    }

    using loomSensor::SampleAverage;
    SampleAverage pm1, pm25, pm4, pm10, humidity, temperature, voc, nox, carbonDioxide;
    SampleAverage number05, number1, number25, number4, number10;
    // Keep raw values until sentinel checks. The installed driver's float converters turn
    // 0xFFFF/0x7FFF into plausible numbers instead of marking them unavailable.
    uint16_t rawPm1 = UINT16_MAX, rawPm25 = UINT16_MAX, rawPm4 = UINT16_MAX;
    uint16_t rawPm10 = UINT16_MAX, rawCo2 = UINT16_MAX;
    int16_t rawHumidity = INT16_MAX, rawTemperature = INT16_MAX;
    int16_t rawVoc = INT16_MAX, rawNox = INT16_MAX;
    uint16_t rawNumber05 = UINT16_MAX, rawNumber1 = UINT16_MAX, rawNumber25 = UINT16_MAX;
    uint16_t rawNumber4 = UINT16_MAX, rawNumber10 = UINT16_MAX;
    uint8_t completeReads = 0;
    uint8_t consecutiveFailures = 0;

    LOG(F("Reading ten fresh SEN66 samples..."));
    for (uint8_t sample = 0; sample < SAMPLE_COUNT; ++sample) {
        delay(
            1050); // Sampling interval is 1 +/- 0.03 seconds; do not average the same update twice.
        uint8_t padding = 0;
        bool dataReady = false;
        int16_t error = sen66.getDataReady(padding, dataReady);
        if (error != 0 || padding != 0) {
            ERRORF("SEN66 data-ready check failed: %d", error);
            if (++consecutiveFailures >= 3) {
                break;
            }
            continue;
        }
        if (!dataReady) {
            if (++consecutiveFailures >= 3) {
                break;
            }
            continue;
        }
        error = sen66.readMeasuredValuesAsIntegers(rawPm1, rawPm25, rawPm4, rawPm10, rawHumidity,
                                                   rawTemperature, rawVoc, rawNox, rawCo2);
        if (error != 0) {
            ERRORF("SEN66 read failed: %d", error);
            if (++consecutiveFailures >= 3) {
                break;
            }
            continue;
        }
        consecutiveFailures = 0;
        ++completeReads;
        // Each field has its own count. A valid zero or negative temperature is still a reading;
        // missing CO2/NOx during startup must not hide valid temperature, humidity or PM data.
        pm1.addUnsigned(rawPm1, 10.0f);
        pm25.addUnsigned(rawPm25, 10.0f);
        pm4.addUnsigned(rawPm4, 10.0f);
        pm10.addUnsigned(rawPm10, 10.0f);
        humidity.addSigned(rawHumidity, 100.0f);
        temperature.addSigned(rawTemperature, 200.0f);
        voc.addSigned(rawVoc, 10.0f);
        nox.addSigned(rawNox, 10.0f);
        carbonDioxide.addUnsigned(rawCo2);
        LOOM_FEED_WATCHDOG(); // A CRC-checked measurement block was read, even if a field is
                              // missing.

        if (measurePM && readNumVals) {
            error = sen66.readNumberConcentrationValuesAsIntegers(
                rawNumber05, rawNumber1, rawNumber25, rawNumber4, rawNumber10);
            if (error == 0) {
                number05.addUnsigned(rawNumber05, 10.0f);
                number1.addUnsigned(rawNumber1, 10.0f);
                number25.addUnsigned(rawNumber25, 10.0f);
                number4.addUnsigned(rawNumber4, 10.0f);
                number10.addUnsigned(rawNumber10, 10.0f);
            } else {
                ERRORF("SEN66 number concentration read failed: %d", error);
            }
        }
    }

    massConcentrationPm1p0 = pm1.result();
    massConcentrationPm2p5 = pm25.result();
    massConcentrationPm4p0 = pm4.result();
    massConcentrationPm10p0 = pm10.result();
    ambientHumidity = humidity.result();
    ambientTemperature = temperature.result();
    vocIndex = voc.result();
    noxIndex = nox.result();
    co2 = carbonDioxide.hasSamples() ? static_cast<uint16_t>(carbonDioxide.result()) : UINT16_MAX;
    numConcentrationPm0p5 = number05.result();
    numConcentrationPm1p0 = number1.result();
    numConcentrationPm2p5 = number25.result();
    numConcentrationPm4p0 = number4.result();
    numConcentrationPm10p0 = number10.result();
    if (consecutiveFailures >= 3) {
        WARNING(F("SEN66 sample window stopped after three unavailable/failed reads."));
        needsReinit =
            true; // Stop predictably before a missing sensor consumes the watchdog period.
    }
    if (completeReads == 0) {
        ERROR(F("No SEN66 measurement blocks collected; readings remain unavailable."));
        needsReinit = true; // Try recovery on the next wake instead of reporting zero pollution.
    }
    logDeviceStatus();
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::package() {
    FUNCTION_START(this);
    JsonObject json = manInst->get_data_object(getModuleName());

    if (measurePM) {
        json["PM1_0"] = (isnan(massConcentrationPm1p0) ? -1 : massConcentrationPm1p0);
        json["PM2_5"] = (isnan(massConcentrationPm2p5) ? -1 : massConcentrationPm2p5);
        json["PM4_0"] = (isnan(massConcentrationPm4p0) ? -1 : massConcentrationPm4p0);
        json["PM10_0"] = (isnan(massConcentrationPm10p0) ? -1 : massConcentrationPm10p0);

        if (readNumVals) {
            json["N_PM0_5"] = (isnan(numConcentrationPm0p5) ? -1 : numConcentrationPm0p5);
            json["N_PM1_0"] = (isnan(numConcentrationPm1p0) ? -1 : numConcentrationPm1p0);
            json["N_PM2_5"] = (isnan(numConcentrationPm2p5) ? -1 : numConcentrationPm2p5);
            json["N_PM4_0"] = (isnan(numConcentrationPm4p0) ? -1 : numConcentrationPm4p0);
            json["N_PM10_0"] = (isnan(numConcentrationPm10p0) ? -1 : numConcentrationPm10p0);
        }
    }

    json["AmbientHumidity"] = (isnan(ambientHumidity) ? -1 : ambientHumidity);
    json["AmbientTemperature"] = (isnan(ambientTemperature) ? -1 : ambientTemperature);
    json["VocIndex"] = (isnan(vocIndex) ? -1 : vocIndex);
    json["NoxIndex"] = (isnan(noxIndex) ? -1 : noxIndex);
    json["CO2"] = co2 == UINT16_MAX ? -1 : static_cast<int32_t>(co2);
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::adjustTempOffset(int16_t offset, int16_t slope, uint16_t timeConstant) {
    FUNCTION_START(this);

    if (moduleInitialized) {
        uint16_t error = sen66.setTemperatureOffsetParameters(offset, slope, timeConstant, 0);
        if (error) {
            ERRORF("Failed to adjust SEN66 sensor offset. Error: %u", error);
        }
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::logDeviceStatus() {
    FUNCTION_START(this);

    SEN66DeviceStatus deviceStatus = {};
    uint16_t error = sen66.readDeviceStatus(deviceStatus);

    if (!error) {
        LOGF("Device status: 0x%08lX", static_cast<unsigned long>(deviceStatus.value));
    } else {
        ERRORF("Error logging SEN66 device status: %u", error);
    }
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////////////////////////
void Loom_SEN66::resetValuesForMeasure() {
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
    ambientHumidity = NAN;
    ambientTemperature = NAN;
    vocIndex = NAN;
    noxIndex = NAN;
    co2 = UINT16_MAX;
    FUNCTION_END;
}
////////////////////////////////////////////////////////////////////////////////////////////////////
