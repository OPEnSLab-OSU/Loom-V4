/*
In lab use case example for the WeatherChimes Roundhouse edition project.
This project uses SDI12, Analog, and I2C communication for its sensors.
*/

//File containing WIFI and MQTT creds
//This file must be present in the folder with this .ino file
//You should see this file at the top of the arduino application as another tab
#include "arduino_secrets.h"

//Loom required libraries
#include <Loom_Manager.h>
#include <Logger.h>

//Libraries required sensors
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Sensors/I2C/Loom_TSL2591/Loom_TSL2591.h>
#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>
#include <Hardware/Loom_TippingBucket/Loom_TippingBucket.h>
#include <Hardware/Loom_Anemometer/Loom_Anemometer.h>
#include <Hardware/Loom_WindVane/Loom_WindVane.h>

//Communication libraries
#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

//Pin for the tipping bucket ISR. 
#define Tip_Pin A5

// Sample flag is set to 1 for first cycle and upon device awakening from sleep, but false at the end of the cycle.
volatile bool alarmFlag = true;  
//tipFlag is set true when the Tip_ISR is ran upon a tip occuring
volatile bool tipFlag = false;

TimeSpan sleepInterval; 

Manager manager("Chimes_Name", 2);

//Create a new Hypnos object
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

//Analog for reading battery voltage, A7 is used for this
Loom_Analog analog(manager);

//Create sensor classes I2C
//This will only be used for non-mux boards, remove once officially swapped to mux board and uncomment the below line.
Loom_SHT31 sht(manager);
Loom_TSL2591 tsl(manager);
Loom_MS5803 ms(manager, 118);

//For mux, 0x76 = ms5803 on board, 0x29 = tsl2591, 0x44 = sht31
//Loom_Multiplexer mux(manager, {0x76, 0x29, 0x44});

//Create sensor classes SDI-12
//to be finished

//Create sensor classes analog
Loom_TippingBucket bucket(manager, COUNTER_TYPE::MANUAL, 0.01f);
Loom_Anemometer anemometer(manager, A1);
Loom_WindVane windvane(manager, A0);


//Create wifi class for upload communication
Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS);
Loom_MongoDB mqtt(manager, wifi, SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS, PROJECT);

//Setting up Wake_ISR
void Wake_ISR() { 
  alarmFlag = true;
  hypnos.wakeup();
}

//Setting up Tip_ISR
void Tip_ISR() {
  tipFlag = true; 
}

void setup() { 
  //Enable debug SD logging and function summaries
  ENABLE_SD_LOGGING;
  ENABLE_FUNC_SUMMARIES;

  //Set Tip_Pin to be input
  pinMode(Tip_Pin, INPUT);

  manager.beginSerial();

  //Both power rails should be on when awake
  hypnos.setWakeConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

  //Both power rails should be on when asleep
  hypnos.setSleepConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

  //Enable the hypnos board
  hypnos.enable();

  //Pull sleep interval from sd card
  sleepInterval = hypnos.getConfigFromSD("SD_config.json");

  //Give the bucket an instance of the hypnos
  bucket.setHypnosInstance(hypnos);

  //Initialize all in-use modules
  manager.initialize();

  //Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(Wake_ISR);

  //Attach the tipping bucket interrupt
  attachInterrupt(Tip_Pin, Tip_ISR, FALLING);
}

void loop() {
  //Set up two if loops so that parts of the loop only run when a tip has occured
  //and the reset of the loop only runs whne the device has been awoken.
  if(alarmFlag) {
    //Enable the dog for safety
    Watchdog.enable(16000);
    //Pet the dog a lot
    Watchdog.reset();

    detachInterrupt(Tip_Pin);

    //Pet the dog a lot
    Watchdog.reset();

    //Set alarm time to the desired length based on what was pulled from the SD card
    hypnos.setInterruptDuration(sleepInterval);

    //Measure and package data
    manager.measure();
    manager.package();

    //Pet the dog a lot
    Watchdog.reset();

    //Print the current JSON packet
    manager.display_data();

    //Log the data to the SD card
    hypnos.logToSD();

    //Disarm the dog
    Watchdog.disable();

    //Publish via WiFi
    mqtt.publish();

    //Reattach the sleep interrupt
    hypnos.reattachRTCInterrupt();

    //Reattach the tip interrupt
    attachInterrupt(Tip_Pin, Tip_ISR, FALLING);

    //Reset arlarmFlag so this part of the loop only runs the next time the device is awoken
    alarmFlag = false;
  }

  if(tipFlag) {
    //Detach the interrupt so that this code is not interrupted
    detachInterrupt(Tip_Pin);

    //Physical indication
    digitalWrite(LED_BUILTIN, HIGH);
    delay(20);

    //Add a tip to the counter
    bucket.incrementCount();

    //Reset the tipFlag so that this part of the loop only runs the next time there is a tip
    tipFlag = false;

    //Reset led to low
    digitalWrite(LED_BUILTIN, LOW);

    //Reattach the interrupt so that the next tip is recognized
    attachInterrupt(Tip_Pin, Tip_ISR, FALLING);
  }

  //Set the hypnos to sleep
  hypnos.sleep(false);
}

