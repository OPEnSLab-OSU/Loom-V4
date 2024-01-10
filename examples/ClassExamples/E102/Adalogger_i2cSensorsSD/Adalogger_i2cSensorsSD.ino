/**
 * This is an example use case for using the Adalogger with a DS3231 and SD card
 * Sketch will log an i2c sensor 
 * NOTE: THIS EXAMPLE DOESN"T WAIT FOR SERIAL AFTER SLEEPING
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/I2C/Loom_STEMMA/Loom_STEMMA.h>

Manager manager("102Test", 1);

// Register STEMMA Soil Moisture i2c sensor with the manager
Loom_STEMMA stemma(manager);

// Create a new Hypnos object setting the version to determine the SD Chip select pin, and starting without the SD card functionality
//Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time = false, bool useSD = true)
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::ADALOGGER, TIME_ZONE::PST, true);

void setup() {

  // Start the serial interface
  manager.beginSerial();

  // Enable the hypnos rails
  hypnos.enable();

  // Called after enable
  manager.initialize();

  // Register the ISR and attach to the interrupt
//  hypnos.registerInterrupt(isrTrigger);
}

void loop() {

  // Measure and package data
  manager.measure();
  manager.package();

/*
  // An example of manually adding data fields to the Data packet for display and logging
  manager.addData("Test", "Test1", 31);
  manager.addData("Test", "Test2", 34);
*/
 
  // Print the current JSON packet
  manager.display_data();            

  // Log the data to the SD card              
  hypnos.logToSD();

  // Wait for 5 seconds
  manager.pause(5000);

}