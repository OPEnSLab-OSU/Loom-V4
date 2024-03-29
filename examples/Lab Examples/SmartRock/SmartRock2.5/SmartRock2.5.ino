/**
 * SmartRock 2.5 (2020)
 * 
 * In lab use case example for the SmartRock project, this version does not use an ADS115
 * 
 * This project uses a hypnos and a MS5803
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>

Manager manager("Device", 1);

// Create a new Hypnos object
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2, TIME_ZONE::PST);

// Analog with A0 and A2 enabled
Loom_Analog analog(manager, A0, A2);
Loom_MS5803 ms(manager, 119);

TimeSpan sleepInterval;

// Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}

void setup() {

  // Wait 20 seconds for the serial console to open
  manager.beginSerial();

  // Enable the hypnos rails
  hypnos.enable();
  manager.initialize();

  sleepInterval = hypnos.getSleepIntervalFromSD("SD_config.json");
  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);
}

void loop() {
  
  // Set the RTC interrupt alarm to wake the device in 10 seconds, at the top to schedule next interrupt asap
  hypnos.setInterruptDuration(sleepInterval);

  // Measure and package the data
  manager.measure();
  manager.package();

  // Print the current JSON packet
  manager.display_data();            

  // Log the data to the SD card              
  hypnos.logToSD();


  // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();
  
  // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep(false);
}