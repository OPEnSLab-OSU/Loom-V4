/**
 * STEMMA Example code
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>
#include <Logger.h>

#include <Sensors/I2C/Loom_STEMMA/Loom_STEMMA.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>

Manager manager("YourName", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin, and starting without the SD card functionality
//Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time = false, bool useSD = true)
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::ADALOGGER, TIME_ZONE::PST, true);

// Reads the battery voltage
Loom_STEMMA stemma(manager);

// Read the battery voltage and A0
Loom_Analog analog(manager, A0);

void setup() {

  // This will log all LOG, ERROR, WARNING and any other logging calls to a file on the SD card
  ENABLE_SD_LOGGING;
  // Enables debug memory usage function summaries that will be logged to the SD card
  ENABLE_FUNC_SUMMARIES;

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

  // Log to the SD card twice and then lay dormant
  hypnos.logToSD();

  // Wait for 5 seconds
  manager.pause(5000);
}
