/**
 * In lab use case example for the SmartRock project
 * IN DEVELOPMENT CODE - MAY BE UNSTABLE
 * This project uses a hypnos, an ADS1115 and a MS5803
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

// Includede Libraries, Mostly OPEnS LOOM

// SmartRock 2026 deployment sketch.
#include <Loom_Manager.h>
#include <Logger.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/I2C/Loom_ADS1115/Loom_ADS1115.h>
#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Wire.h>
#include "Adafruit_VCNL4010.h"
//============================================================

// Loom Manager, Hypnos, and Sensor Constructors

// OPEnS Loom Constructors
Manager manager("US_Virgin_Islands_5", 1);
Loom_Analog analog(manager);

// OPEnS Hypnos Constructors
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
TimeSpan sleepInterval;

// Sensor Module Constructors from Loom
Loom_ADS1115 ads(manager);
Loom_MS5803 ms(manager, 119);
Adafruit_VCNL4010
    vcnl; // VCNL4010 is not yet a Loom sensor module, so we are using the Adafruit library

bool vcnlFlag = false;
//============================================================

// Smart Rock Specific Function Declares

// Called when the interrupt is triggered
void isrTrigger();

// Measures and packages sensor readings which are displayed to the serial monitor and sent to the
// SD card
void take_data(float, float, float, float);
//============================================================

// Setup runs once, initializes the Hypnos rails and VCNL4010, and reads SD_config.json to get the
// sleep interval
void setup() {

    // Wait 20 seconds for the serial console to open
    Logger::getInstance()->setDebugOutput(false);
    manager.beginSerial(false);
    hypnos.setCompileTime(__DATE__, __TIME__);

    // Enable the hypnos rails
    hypnos.enable();

    // Allow the switched sensor rail to stabilize before probing the ADS1115.
    delay(1500);

    // SmartRock uses the standard I2C bus at 100 kHz.
    Wire.begin();
    Wire.setClock(100000);

    manager.initialize();

    // Gets sleep interval from SD card
    sleepInterval = hypnos.getConfigFromSD("SD_config.json");
    // Register the ISR and attach to the interrupt
    hypnos.registerInterrupt(isrTrigger);

    // VCNL4010 Initialization indicator
    if (!vcnl.begin()) {
        Serial.println("VCNL4010 Not Found");
        vcnlFlag = false;
    } else {
        Serial.println("VCNL4010 Initialized");
        vcnlFlag = true;
    }
}
//============================================================

// SD_config.json sets the interval between sample starts, including measurement/logging time.
void loop() {

    // Start the clock BEFORE taking data. Later cycles keep this UTC phase instead of adding
    // sensor work to the configured period. Long cycles skip missed slots before sleep.
    if (!hypnos.setSampleInterval(sleepInterval)) {
        manager.pause(60000);
        hypnos.enable();
        return;
    }

    // Set constants, these should be changed to match the intended use

    // Experimentally determined EC and Turbidity calibration coefficients
    // These are different for each Smart Rock, insert the proper values from calibration
    float EC_slope = 0.00;               // EC calibration slope
    float EC_intercept = 0.00;           // EC calibration y-intercept
    float Turbidity_slope = .18688;      // Turbidity calibration slope
    float Turbidity_intercept = -549.82; // Turbidity calibration y-intercept

    // This is the wakeup cycle, everything outside of this scope happens while the device is
    // "asleep"

    // Smart Rock function to measure, package, calibrate, and display data
    take_data(EC_slope, EC_intercept, Turbidity_slope, Turbidity_intercept);

    hypnos.sleep();
    //============================================================
}
//============================================================

// Smart Rock specific functions

// Called when the interrupt is triggered
void isrTrigger() { hypnos.wakeup(); }
//===========================

// Smart Rock function to measure, package, calibrate, and display data
// m: calibration slopes (EC and Turbidity)
// b: calibration y-intercepts (EC and Turbidity)
void take_data(float ECm, float ECb, float Tm, float Tb) {

    // Measure and package the data from the Loom modules into JSON packet
    manager.measure();
    manager.package();

    //===========================
    // Applying Calibration Equations

    // This equation converts the measured conductance into EC in uS/cm
    const float voltage = ads.getAnalog(1);
    const float EC = voltage != 0 ? ads.getAnalog(2) / voltage * ECm + ECb : NAN;
    float turbidity = NAN;
    uint16_t proximity = 0;

    if (vcnlFlag) {
        // This equation converts the measured infrared backscatter (proximity) into Turbidity in
        // NTU
        proximity = vcnl.readProximity();
        turbidity = proximity * Tm + Tb;
    }

    //===========================

    // Adds data under "Sensor Values" to the JSON packet
    manager.addData("MS5803", "Pressure", ms.getPressure());       // MS5803 Pressure
    manager.addData("MS5803", "Temperature", ms.getTemperature()); // MS5803 Temperature

    if (vcnlFlag) {
        manager.addData("vcnl4010", "Ambient Light", vcnl.readAmbient()); // VCNL4010 Light
        manager.addData("vcnl4010", "Proximity", proximity);   // VCNL4010 Proximity
    }

    manager.addData("Analog Values", "Conductivity", EC); // Calibrated EC in uS/cm
    manager.addData("Analog Values", "Turbidity", turbidity);  // Calibrated Turbidity

    if (!hypnos.logToSD()) {
        ERROR(F("SmartRock sample could not be fully saved to SD."));
    }

}
//============================================================
