/**
 * In lab use case example for the SmartRock project
 * IN DEVELOPMENT CODE - MAY BE UNSTABLE
 * This project uses a hypnos, an ADS1115 and a MS5803
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

// Includede Libraries, Mostly OPEnS LOOM 

#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/I2C/Loom_ADS1115/Loom_ADS1115.h>
#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>
#include <Sensors/I2C/Loom_VCNL4010/Loom_VCNL4010.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Wire.h>
//============================================================

// Loom Manager, Hypnos, and Sensor Constructors

  //OPEnS Loom Constructors
Manager manager("Data", 1);
Loom_Analog analog(manager);

  // OPEnS Hypnos Constructors
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
TimeSpan sleepInterval;

  // Sensor Module Constructors from Loom
Loom_ADS1115 ads(manager, (byte)ADS1X15_ADDRESS, false, AnalogChannelEnable::ENABLE_1_2, false);
Loom_MS5803 ms(manager, 119);
Loom_VCNL4010 vcnl(manager);
//============================================================

// Smart Rock Specific Function Declares

  // Called when the interrupt is triggered 
void isrTrigger();

  // Measures and packages sensor readings which are displayed to the serial monitor and sent to the SD card
void take_data(float, float, float, float, bool);
//============================================================

// Setup runs once, initializes the Hypnos rails and VCNL4010, and reads SD_config.json to get the sleep interval
void setup() {

    // Wait 20 seconds for the serial console to open
  manager.beginSerial();

    // Enable the hypnos rails
  hypnos.enable();
  manager.initialize();

    // Gets sleep interval from SD card
  sleepInterval = hypnos.getConfigFromSD("SD_config.json");
    // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);
}
//============================================================

// Runs this loop indefinitely, sleeps for the interval set in SD_config.json between each iteration
void loop() {

// Set constants, these should be changed to match the intended use

    // Experimentally determined EC and Turbidity calibration coefficients
    // These are different for each Smart Rock, insert the proper values from calibration
  float EC_slope = 0.00;      // EC calibration slope 
  float EC_intercept = 0.00;  // EC calibration y-intercept 
  float Turbidity_slope = 0.1574;      // Turbidity calibration slope
  float Turbidity_intercept = -496.06 - 40;  // Turbidity calibration y-intercept
  
    // Troubleshooting mode enables extra status prints in the serial Monitor
  bool troubleshooting;
//============================================================

// This is the wakeup cycle, everything outside of this scope happens while the device is "asleep"

    // Troubleshooting print 1
  if(troubleshooting_mode==1){Serial.println("Woke Up!");}

    // Smart Rock function to measure, package, calibrate, and display data
  take_data(EC_slope, EC_intercept, Turbidity_slope, Turbidity_intercept, troubleshooting_mode);

    // Troubleshooting print 7
  if(troubleshooting_mode==1){Serial.println("Setting Alarm");}

    // Set the RTC interrupt alarm to wake the device after the set sleep interval
  hypnos.setInterruptDuration(sleepInterval);

    // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();
  
    // Troubleshooting print 8
  if(troubleshooting_mode==1){Serial.println("Going to Sleep");}
  
    // Put the device to sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep(false);  // To disable slee manager.addData("MS5803", "Pressure", ms.getPressure());          // MS5803 Pressure

//============================================================

}
//============================================================

// Smart Rock specific functions

  // Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}
//===========================

  // Smart Rock function to measure, package, calibrate, and display data 
  // m: calibration slopes (EC and Turbidity)
  // b: calibration y-intercepts (EC and Turbidity)
void take_data(float ECm, float ECb, float Tm, float Tb, bool troubleshooting_mode){

    // Troubleshooting print 2
  if(troubleshooting_mode==1){Serial.println("Begin Taking Data");}

    // Measure and package the data from the Loom modules into JSON packet
  manager.measure();
  manager.package();

    // Troubleshooting print 3
  if(troubleshooting_mode==1){Serial.println("Measured Data");}
//===========================
  // Applying Calibration Equations
  
    // This equation converts the measured conductance into EC in uS/cm
  float EC = ((ads.getAnalog(2)/ads.getAnalog(1)) * ECm + ECb); // ECm and ECb are the passed in slope and y-intercept from the beginning of void loop()
    // This equation converts the measured infrared backscatter (proximity) into Turbidity in NTU
  float Turb = ((vcnl.readProximity()) * Tm + Tb); // Tm and Tb are the passed in slope and y-intercept from the beginning of void loop()
    // Troubleshooting print 4
  if(troubleshooting_mode==1){Serial.println("EC and Turbidity Values Calculated");}
 //===========================
  
    // Adds data under "Sensor Values" to the JSON packet
  manager.addData("Analog Values","Conductivity", EC);              // Calibrated EC in uS/cm
  manager.addData("Analog Values","Turbidity", Turb);              //Calibrated Turbidity

    // Troubleshooting print 5
  if(troubleshooting_mode==1){Serial.println("Data Added to Packet");}

    // Print the current JSON packet to Serial Monitor and log to SD card
  manager.display_data();                   
  hypnos.logToSD();
  
    // Troubleshooting print 6
  if(troubleshooting_mode==1){Serial.println("Packet Logged to SD Card");}
}
//============================================================
