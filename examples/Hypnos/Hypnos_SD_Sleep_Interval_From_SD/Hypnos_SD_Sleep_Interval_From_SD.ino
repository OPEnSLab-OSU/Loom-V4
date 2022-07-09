/**
 * This is an example use case for the Hypnos board's sleep functionality in addition to the SD logging
 * This example increments a counter each power cycle logging the data to the SD card.
 * 
 * NOTE: THIS EXAMPLE DOESN"T WAIT FOR SERIAL AFTER SLEEPING
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

Manager manager("Device", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin, and starting without the SD card functionality
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2, TIME_ZONE::PST);

TimeSpan sleepInterval;

// Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}

void setup() {

  // Start and wait for the user to open the Serial monitor
  Serial.begin(115200);
  while(!Serial);

  // Enable the hypnos rails
  hypnos.enable();

  sleepInterval = hypnos.getSleepIntervalFromSD("SD_config.json");

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);
}

void loop() {
  
  // Print the current JSON packet
  manager.display_data();            

  // Log the data to the SD card              
  hypnos.logToSD();

  // Set the RTC interrupt alarm to wake the device in 10 seconds
  hypnos.setInterruptDuration(sleepInterval);

  // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();
  
  // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep(false);
  testCounter++;
}