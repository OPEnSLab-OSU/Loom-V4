/**
 * This is a lab example for Evaporometer
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/SPI/Loom_ADS1232/Loom_ADS1232.h>

Manager manager("Device", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin, and starting without the SD card functionality
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
Loom_ADS1232 ads(manager);

// Called when the interrupt is triggered
void isrTrigger(){
  hypnos.wakeup();
}

void setup(){

  // Start the serial interface
  manager.beginSerial();

  // Enable the hypnos rails
  hypnos.enable();

  // Initialize the manager
  manager.initialize();

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);
}

void loop(){

  // Set the RTC interrupt alarm to wake the device in 30 seconds, at the top to schedule next interrupt asap
  hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 30));

  // Measure and package the data from the sensors
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