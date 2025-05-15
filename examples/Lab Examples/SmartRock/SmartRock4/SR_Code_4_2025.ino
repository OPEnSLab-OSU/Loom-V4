/**
 * In lab use case example for the SmartRock project
 * IN DEVELOPMENT CODE - MAY BE UNSTABLE
 * This project uses a hypnos, an ADS1115 and a MS5803
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#include <Sensors/I2C/Loom_ADS1115/Loom_ADS1115.h>
#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>

//------------------------------------------------------------
#include <Wire.h>
#include "Adafruit_VCNL4010.h"

Adafruit_VCNL4010 vcnl;
//------------------------------------------------------------

Manager manager("Data", 1);

  // Create a new Hypnos object
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

  // Sensors to use
Loom_ADS1115 ads(manager);
Loom_MS5803 ms(manager, 119);

Loom_Analog analog(manager);

TimeSpan sleepInterval;
//hgkjl
  // Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}

//************ Chris' Function Declares ************

void take_data(float, float, int, int);
float calculate_EC(float,float);

//**************************************************

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

//------------------------------------------------------------
  if (! vcnl.begin()){
    Serial.println("Proximity sensor not found :(");
    while (1);
  }
  Serial.println("Found VCNL4010");



}

void loop() {

//************************************* Chris' main ***************************************

    // EC_slope: EC calibration slope
    // EC_intercept: EC calibration y-intercept
    // count: Number of data points to take
    // interval: Time interval between data collection in milliseconds
  float EC_slope = 1012.7;        // <-----**************************<-----
  float EC_intercept = 24.939;  // <----- Configure take_data here <-----
  int count = 1;                  // <-----                          <-----
  int interval = 100;             // <-----**************************<-----

    //Troubleshooting print
  //Serial.print("Main pause");
  manager.pause(5000);

  take_data(EC_slope, EC_intercept, count, interval);

    // Set the RTC interrupt alarm to wake the device in 10 seconds
  hypnos.setInterruptDuration(sleepInterval);

    // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();

    // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep(false);
    
//*****************************************************************************************

}

//************************************ Chris' Functions ***********************************

  // take_data collects data for all sensors a specified amount of times and logs them to the SD card
  // m: EC calibration slope
  // b: EC calibration y-intercept
  // count: Number of data points to take
  // interval: Time interval between data collection in milliseconds
void take_data(float m, float b, int count, int interval){

      //Troubleshooting print
    Serial.println("taking data");
    
    for(int i = 0 ; i < count ; i++ ){

          // Measure and package the data
        manager.measure();
        manager.package();

          // calculates the EC in uS/cm   ***Chris Function***
        float EC = calculate_EC(m, b);

        Serial.println(ads.getAnalog(1));
        Serial.println(ads.getAnalog(2));
        
          // Adds data under "Analog Values" to Excel and Serial Monitor
        manager.addData("Analog Values","Conductivity (uS/cm)", EC);
        manager.addData("Analog Values", "Battery Voltage", analog.getBatteryVoltage());

                       Serial.println("it got here 1"); //troubleshooting comment (made it this far)

        manager.addData("vcnl4010","Ambient Light", vcnl.readAmbient());
        manager.addData("vcnl4010","Proximity", vcnl.readProximity());

                        Serial.println("it got here 2"); //troubleshooting comment (didn't make it this far)

        manager.addData("MS5803", "Pressure", ms.getPressure());
        manager.addData("MS5803", "Temperature", ms.getTemperature());

          // Print the current JSON packet to Serial Monitor and log to SD card
                        Serial.println("it got here 3"); //troubleshooting comment (didn't make it this far)
        manager.display_data();                   
        hypnos.logToSD();
                        Serial.println("it got here 4"); //troubleshooting comment (didn't make it this far)
        
          // Pause for specified interval between data points
        manager.pause(interval);
    }
}

  // calculaute_EC calculates the EC in uS/cm, can be set to remove outliars
  // m: EC calibration slope
  // b: EC calibration y-intercept
float calculate_EC(float m, float b){

      //Troubleshooting print
    Serial.print("calculating EC");

    float curr = ads.getAnalog(2);
    float volt = ads.getAnalog(1);
    
      // Calculates EC value in uS/cm
    float ratio = curr / volt;
    float EC = ( ( ratio * m ) + b );
    Serial.println(ratio);
    //Serial.print(EC);

      //remove values too low for OPAMP voltage threshold
    if(curr <= 0 || volt <= 0){
          EC = 0;
    } 
    return EC;
}
//*****************************************************************************************
