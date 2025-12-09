/**
 * This is an example use case for the Hypnos board's sleep functionality
 * This allows the user to put the Feather into a deep sleep disabling power to all sensors and then resume operation after a given length of time
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>

#define DS3231_ADDRESS 0x68
#define DS3231_STATUSREG 0x0F

// Manager to control the device
Manager manager("Device", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin, and starting without the SD card functionality
//Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time = false, bool useSD = true)
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, false, false);

// maybe don't need to have full heartbeat mode from get go.
Loom_LoRa lora(manager);

// Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}

void setup() {
  // Enable the hypnos rails
  hypnos.enable();
  
  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);

  // Start the serial interface
  manager.beginSerial();
  manager.initialize();

  // initialize heartbeat operations
  uint8_t destAddr = 0;
  uint32_t hbInterval_s = 60;
  uint32_t normalInterval_s = 35;
  lora.heartbeatInit(destAddr, hbInterval_s, normalInterval_s, &hypnos);
}

void loop() {
  lora.adjustHbFlagFromAlarms();

  // logic to execute this loop
  if(lora.getHeartbeatFlag())
  {
    Serial.println("__________________Hello from Heartbeat Branch________________");
    lora.sendHeartbeat();
  }
  else {
    // do work
    manager.package();
    Serial.println("~~~~~~~~~~~~~~~~~~~~~~~Hello from Normal Work Branch~~~~~~~~~~~~~~~~~~~~");
    lora.send(0);
  }

  // set up both alarms for the heartbeat mode using the specified intervals earlier.
  lora.ensureHeartbeatHypnosAlarmsActive();

  // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();

  Serial.flush(); // Forces all serial log messages in buffer to print now.
  
  // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep();

  delay(100);
}