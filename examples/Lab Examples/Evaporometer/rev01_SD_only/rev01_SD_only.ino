/*
 * Title: Evaporometer SD Card Logging Only 
 *
 * Description: Uses ADS1232 to measure voltage 
 *              across a load cell. Uses on board ADC to measure voltage
 *              across a thermistor, logs data to SD card.
 *
 * Date: Feb 2, 2025
 */

// Loom manager must be included first
#include <Loom_Manager.h>
#include <Sensors/SPI/Loom_ADS1232/ADS1232_Lib_Fixed.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Loom_Hypnos/SDManager.h>
#include <algorithm>

// --------------------------------------------------------------------
//                          Pin Definitions
// --------------------------------------------------------------------
constexpr int PIN_PDWN      = A5;
constexpr int PIN_SCLK      = A4;
constexpr int PIN_DOUT      = A3;
constexpr int PIN_VBAT      = A7;
constexpr int PIN_THERM     = A1;

constexpr int PIN_SD_1      = 23;
constexpr int PIN_SD_2      = 24;
constexpr int PIN_SD_3      = 11;

// --------------------------------------------------------------------
//                           Constants
// --------------------------------------------------------------------

// Total time to wait between measurements
constexpr int TOTAL_PERIOD_SEC = 30;

// Calibration constants
constexpr long  LOAD_CELL_OFFSET    = 9617696; 
constexpr float LOAD_CELL_SCALE     = 2093; 

// --------------------------------------------------------------------
//   Temperature-Correction Parameters
//   w_drift = m * T + b
//   w_corrected = w_measured - w_drift - w_ref
//   For now, we assume w_ref = 0
// --------------------------------------------------------------------
constexpr float TEMP_CORR_M         = -0.12493f;
constexpr float TEMP_CORR_B         = 117.73f;
constexpr float REF_WEIGHT          = 0.0f;

// Battery measurement constants
// (Resistor divider = 2, 3.3V reference, 10-bit ADC)
constexpr float VBAT_SCALE_FACTOR   = (2.0f * 3.3f / 1024.0f);

// Number of consecutive weight readings to take
constexpr int   NUM_WEIGHT_SAMPLES  = 3;

// Number of units averaged for each reading
constexpr int   NUM_READINGS        = 10;

// File name for logging
constexpr char  LOG_FILE[]          = "Device0.csv";

// --------------------------------------------------------------------
//                       Global Objects
// --------------------------------------------------------------------
Manager     manager("Device", 1);
ADS1232_Lib_Fixed ads(PIN_PDWN, PIN_SCLK, PIN_DOUT);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true, false);
SDManager   sd(&manager, 11);
bool sdReady = false;

// --------------------------------------------------------------------
//                    Forward Declarations
// --------------------------------------------------------------------
bool logData(int rawThermValue, float temperature, float weight, float correctedWeight, float vbat);
bool measureWeight(float &weight);
float measureBatteryVoltage();
void isrTrigger();
float computeCorrectedWeight(float wMeasured, float temperature);

// --------------------------------------------------------------------
//                         logData Function
//    Logs weight, thermistor reading, and battery voltage to the
//    SD card with a timestamp from the RTC.
// --------------------------------------------------------------------
bool logData(int rawThermValue, float temperature, float weight, float correctedWeight, float vbat)
{
    DateTime now;
    if (!hypnos.tryGetCurrentTime(now)) {
        return false;
    }
    if (!sdReady) {
        if (!sd.begin()) {
            return false;
        }
        File file = sd.openFile(LOG_FILE);
        const bool needsHeader = !file || file.fileSize() == 0;
        file.close();
        if (needsHeader &&
            !sd.writeLineToFile(LOG_FILE, "time,adc_counts,temperature,weight,corrected_weight,voltage")) {
            return false;
        }
    }

    char timeBuffer[32] = {};
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d %02d/%02d/%02d",
             now.hour(), now.minute(), now.second(),
             now.day(), now.month(), now.year());

    char logLine[128] = {};
    const int length = snprintf(logLine, sizeof(logLine), "%s,%d,%.2f,%.2f,%.2f,%.3f",
                                timeBuffer, rawThermValue, temperature, weight,
                                correctedWeight, vbat);
    sdReady = length > 0 && static_cast<size_t>(length) < sizeof(logLine) &&
              sd.writeLineToFile(LOG_FILE, logLine);
    return sdReady;
}

// --------------------------------------------------------------------
//                       measureWeight Function
//     Takes multiple readings from the load cell, sorts them, and
//     returns the median as a stable measurement.
// --------------------------------------------------------------------
bool measureWeight(float &weight)
{
    if (!ads.power_up()) {
        ads.power_down();
        return false;
    }
    delay(1000); // Allow the load cell to stabilize.
    float weights[NUM_WEIGHT_SAMPLES] = {};

    for (float &reading : weights) {
        reading = ads.units_read(NUM_READINGS);
        if (!ads.last_read_valid()) {
            ads.power_down();
            return false;
        }
    }
    ads.power_down();

    std::sort(weights, weights + NUM_WEIGHT_SAMPLES);
    weight = weights[NUM_WEIGHT_SAMPLES / 2];
    return true;
}

// --------------------------------------------------------------------
//                computeCorrectedWeight Function
//     Takes inputted weight measurement and temperature to correct
//     the weight and then returns the corrected weight.
// --------------------------------------------------------------------
float computeCorrectedWeight(float wMeasured, float temperature)
{
    float wDrift = (TEMP_CORR_M * temperature) + TEMP_CORR_B;
    return (wMeasured - wDrift - REF_WEIGHT);
}

// --------------------------------------------------------------------
//                     measureBatteryVoltage Function
//     Reads the voltage from the battery (via a resistor divider).
//     Prints and returns the measured voltage.
// --------------------------------------------------------------------
float measureBatteryVoltage()
{
    float rawReading = static_cast<float>(analogRead(PIN_VBAT));
    float measuredVbat = rawReading * VBAT_SCALE_FACTOR;
    return measuredVbat;
}

// --------------------------------------------------------------------
//                       ISR Trigger Function
//     Called when RTC interrupt fires to wake up from sleep mode.
// --------------------------------------------------------------------
void isrTrigger()
{
    hypnos.wakeup();
}

// --------------------------------------------------------------------
//                              SETUP
// --------------------------------------------------------------------
void setup()
{
    manager.beginSerial(false);
    hypnos.setCompileTime(__DATE__, __TIME__);

    // Enable Hypnos (power management, RTC, etc.)
    hypnos.enable();

    // Initialize Loom manager
    manager.initialize();

    // Register RTC interrupt service routine for wakeup
    hypnos.registerInterrupt(isrTrigger);

    // Set SD card write pins
    pinMode(PIN_SD_1, OUTPUT);
    pinMode(PIN_SD_2, OUTPUT);
    pinMode(PIN_SD_3, OUTPUT);

    // Set thermistor pin
    pinMode(PIN_THERM, INPUT);

    // Configure the load cell offset/scale
    ads.set_offset(LOAD_CELL_OFFSET);
    ads.set_scale(LOAD_CELL_SCALE);
}

// --------------------------------------------------------------------
//                              LOOP
// --------------------------------------------------------------------
void loop()
{
    if (!hypnos.setSampleInterval(TimeSpan(TOTAL_PERIOD_SEC))) {
        manager.pause(TOTAL_PERIOD_SEC * 1000UL);
        hypnos.enable();
        return;
    }

    float weight;
    if (measureWeight(weight)) {
        const int rawThermValue = analogRead(PIN_THERM);
        const float temperature = static_cast<float>(rawThermValue);
        const float correctedWeight = computeCorrectedWeight(weight, temperature);
        if (!logData(rawThermValue, temperature, weight, correctedWeight, measureBatteryVoltage())) {
            Serial.println(F("Could not confirm the Evaporometer sample was saved to SD."));
        }
    } else {
        Serial.println(F("ADS1232 did not provide a complete weight reading."));
    }
    hypnos.sleep();
}

