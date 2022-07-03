/**
 * In lab use case example for the SmartRock project
 * 
 * This project uses a hypnos, an ADS1115 and a MS5803
 */
#include <Loom_Hypnos.h>
#include <Loom_Manager.h>
#include <Loom_ADS1115.h>
#include <Loom_MS5803.h>

Manager manager("Device", 1);

// Create a new Hypnos object
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2, TIME_ZONE::PST);

// Sensors to use
Loom_ADS1115 ads(manager);
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

  sleepInterval = hypnos.getSleepIntervalFromSD("SD_config.txt");
  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);
}

void loop() {

  // Measure and package the data
  manager.measure();
  manager.package();
  
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
}
