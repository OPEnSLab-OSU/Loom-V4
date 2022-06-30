/**
 * This is an example use case combining the Hypnos sleep functionality with the SDI-12 logging
 * This will collect print data to the serial console and then sleep for 10 seconds
 * 
 * It will wait for you to reopen the Serial monitor when it reconnects before continuing
 * 
 */

#include <Loom_Manager.h>
#include <Loom_SDI12.h>
#include <Loom_Hypnos.h>


// Manager handles all loom simplicity 
Manager manager("Device", 1);

// The manager should be passed into each Loomified sensor to enable easy measure and packing functionality
Loom_SDI12 decagon(manager, 11);

// Use compile time to set the RTC and disable the SD card
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2, TIME_ZONE::PST, false, false);

// Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}

void setup() {
  hypnos.enable();

  manager.beginSerial();    // Open a serial connection and by default wait 20 seconds before printing if the Serial monitor isnt open.
  manager.initialize();     // Initialize in-use modules

  hypnos.registerInterrupt(isrTrigger);
}

void loop() {

  manager.measure();        // Pull the sensors for measurements
  manager.package();        // Package the data into JSON
  manager.display_data();      // Print the current packaged json packet out

  // Set the RTC interrupt alarm to wake the device in 10 seconds
  hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 10));

  // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();
  
  // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep(true);
}