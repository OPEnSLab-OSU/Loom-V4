/**
 * This is an example use case for the Hypnos board's sleep functionality
 * This allows the user to put the Feather into a deep sleep disabling power to all sensors and then resume operation after a given length of time
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>

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
  // Start the serial interface
  manager.beginSerial();
  manager.initialize();

  // Enable the hypnos rails
  hypnos.enable();
  
  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);

  // initialize heartbeat operations
  uint8_t destAddr = 0;
  uint32_t hbInterval_s = 15;
  uint32_t normalInterval_s = 35;
  lora.heartbeatInit(destAddr, hbInterval_s, normalInterval_s);
}

void loop() {
  manager.package();
  manager.display_data();

  // logic to execute this loop
  if(lora.getHeartbeatFlag())
  {
    Serial.println("Within Heartbeat Branch");
    lora.sendHeartbeat();
  }

  else {
    // do work
    Serial.println("Within Normal Work Branch");
    lora.send(0);
  }

  TimeSpan tSpan = lora.hbNextEvent();
  Serial.print("Sleep for: ");
  Serial.print(String(tSpan.seconds()));
  Serial.println(" Seconds");

  // Set the RTC interrupt alarm to wake the device based on preset intervals declared in heartbeatInit();
  hypnos.setInterruptDuration(tSpan);

  // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();
  
  // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep();
}