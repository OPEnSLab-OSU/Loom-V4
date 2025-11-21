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
#include <ADS1232_Lib.h>
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
ADS1232_Lib ads(PIN_PDWN, PIN_SCLK, PIN_DOUT);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true, false);
SDManager   sd(&manager, 11);

// --------------------------------------------------------------------
//                    Forward Declarations
// --------------------------------------------------------------------
void logData(int rawThermValue, float temperature, float weight, float correctedWeight, float vbat);
float measureWeight();
float measureBatteryVoltage();
void isrTrigger();
float computeCorrectedWeight(float wMeasured, float temperature);

// --------------------------------------------------------------------
//                         logData Function
//    Logs weight, thermistor reading, and battery voltage to the
//    SD card with a timestamp from the RTC.
// --------------------------------------------------------------------
void logData(int rawThermValue, float temperature, float weight, float correctedWeight, float vbat)
{
    // Build time string
    char timeBuffer[32] = {};
    DateTime now = hypnos.getCurrentTime();
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d:%02d:%02d %02d/%02d/%02d",
             now.hour(), now.minute(), now.second(),
             now.day(), now.month(), now.year());

    // Convert data to strings
    char weightStr[16] = {};
    snprintf(weightStr, sizeof(weightStr), "%.2f", weight);

    char thermStr[16] = {};
    snprintf(thermStr, sizeof(thermStr), "%d", rawThermValue);

    char vbatStr[16] = {};
    snprintf(vbatStr, sizeof(vbatStr), "%.3f", vbat);

    // Build CSV line: time, weight, therm, battery
    char logLine[128] = {};
    snprintf(logLine, sizeof(logLine),
             "%s,%d,%.2f,%.2f,%.2f,%.3f",
             timeBuffer, rawThermValue, temperature, weight, correctedWeight, vbat);

    // Write to SD
    sd.writeLineToFile(LOG_FILE, logLine);
}

// --------------------------------------------------------------------
//                       measureWeight Function
//     Takes multiple readings from the load cell, sorts them, and
//     returns the median as a stable measurement.
// --------------------------------------------------------------------
float measureWeight()
{
    float weights[NUM_WEIGHT_SAMPLES] = {};

    // Take N measurements
    for(int i = 0; i < NUM_WEIGHT_SAMPLES; i++) {
        weights[i] = ads.units_read(NUM_READINGS);
    }

    // Sort to pick the median
    std::sort(weights, weights + NUM_WEIGHT_SAMPLES);

    // Return the middle element
    return weights[NUM_WEIGHT_SAMPLES / 2];
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
    manager.beginSerial();
    Serial.println("Evaporometer Starting...");

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

    // Start SD manager
    sd.begin();

    // Header for the CSV
    sd.writeLineToFile(LOG_FILE, "time,adc_counts,temperature,weight,corrected_weight,voltage");

    // Set thermistor pin
    pinMode(PIN_THERM, INPUT);

    // Configure the load cell offset/scale
    ads.set_offset(LOAD_CELL_OFFSET);
    ads.set_scale(LOAD_CELL_SCALE);

    Serial.println("End of Setup\n\n");
}

// --------------------------------------------------------------------
//                              LOOP
// --------------------------------------------------------------------
void loop()
{
    ads.power_up();        // Turn on the ADS1232 if needed
    
    // Get start time
    DateTime loopStart = hypnos.getCurrentTime();

    // Measure Weight
    Serial.println();
    Serial.println();
    Serial.println("Taking measurements...");
    manager.pause(1000);   // Delay to allow load cell to stabilize
    float weight = measureWeight();

    // Read Thermistor (currently just an ADC raw value)
    int rawThermValue = analogRead(PIN_THERM);

    // Convert raw therm value to float for correction
    float temperature = static_cast<float>(rawThermValue);

    // Calculate Corrected Weight
    float correctedWeight = computeCorrectedWeight(weight, temperature);

    // Measure Battery Voltage
    float vbat = measureBatteryVoltage();

    // Print to serial
    Serial.print("Weight: ");
    Serial.print(weight);
    Serial.print(" g, Temp Raw: ");
    Serial.print(rawThermValue);
    Serial.print(" (~T: ");
    Serial.print(temperature);
    Serial.print("), Corrected Weight: ");
    Serial.print(correctedWeight);
    Serial.print(" g, Battery: ");
    Serial.print(vbat);
    Serial.println(" V");

    // Log Data to SD
    Serial.println("Logging data...");
    logData(rawThermValue, temperature, weight, correctedWeight, vbat);

    // Get end time
    DateTime loopEnd = hypnos.getCurrentTime();

    // Calculate how long to sleep
    TimeSpan totalTime = loopEnd - loopStart;
    long timeSpent = totalTime.totalseconds();
    long timeToSleep = TOTAL_PERIOD_SEC - timeSpent;
    if (timeToSleep < 0) {
        timeToSleep = 0; // clamp to zero if we overran
    }

    Serial.println("Sleeping...");
    delay(timeToSleep * 1000);
    // // Sleep Setup
    hypnos.setInterruptDuration(TimeSpan(0, 0, 0, timeToSleep));
    hypnos.reattachRTCInterrupt();

    // Go to sleep now
    hypnos.sleep();
}

