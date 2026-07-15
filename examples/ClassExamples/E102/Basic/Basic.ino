/**
 * Author: Chet Udell
 * ENGR 102 Basic Example Code: 
 * Read the Analog A0 channel
 * Display Data on Serial Monitor
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 * Hypnos and Analog 0 are registered with the Manager
 * Setup section begins serial, enabled Hypnos library features, starts the Manager
 * Loop: Manager measures, packages, displays data.
 * Manager.pause delays the processor to pause for 5 seconds and repeats loop from top
 */


#include <Loom_Manager.h>
#include <Logger.h>

#include <Sensors/Loom_Analog/Loom_Analog.h>

Manager manager("YourName", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin, and starting without the SD card functionality
//Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time = false, bool useSD = true)
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::ADALOGGER, TIME_ZONE::PST, true);

// Register analog module with manager. Creates an instance called "analog." Will read battery voltage and A0
Loom_Analog analog(manager, A0);

void setup() {

  // Start the serial interface
  manager.beginSerial();

  // Enable the hypnos rails
  hypnos.enable();

  // Initialize the manager
  manager.initialize();
}

void loop() {
  // put your main code here, to run repeatedly:

  // Measure the data from the sensors
  manager.measure();

  // Package the data into JSON
  manager.package();

  // Print the JSON document to the Serial monitor
  manager.display_data();

  // Wait for 5 seconds
  manager.pause(5000);
}
