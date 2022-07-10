/**
 * This is an example use case for the Hypnos board's sleep functionality in addition to the SD logging
 * This example increments a counter each power cycle logging the data to the SD card.
 * 
 * NOTE: THIS EXAMPLE DOESN"T WAIT FOR SERIAL AFTER SLEEPING
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/I2C/Loom_ADS1115/Loom_ADS1115.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Sensors/SPI/Loom_MAX318XX/Loom_MAX31865.h>

Manager manager("Device", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin, and starting without the SD card functionality
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2, TIME_ZONE::PST);
Loom_ADS1115 ads(manager);
Loom_SHT31 sht(manager);

// Set chip select to pin 9
Loom_MAX31865 max31(manager, 1, 9);

// Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}

void setup() {

  // Start the serial interface
  manager.beginSerial();

  // Enable the hypnos rails
  hypnos.enable();

  // Called after enable
  manager.initialize();

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);
}

void loop() {

  // Measure and package data
  manager.measure();
  manager.package();
  
  // Print the current JSON packet
  manager.display_data();            

  // Log the data to the SD card              
  hypnos.logToSD();

  // Set the RTC interrupt alarm to wake the device in 10 seconds
  hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 5));

  // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();
  
  // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep(false);
}