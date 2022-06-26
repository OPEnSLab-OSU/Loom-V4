/**
 * This is an example use case for the Hypnos board's sleep functionality
 * This allows the user to put the Feather into a deep sleep disabling power to all sensors and then resume operation after a given length of time
 */

#include <Loom_Hypnos.h>


// Create a new Hypnos object setting the version to determine the SD Chip select pin, and starting without the SD card functionality
Loom_Hypnos hypnos(HYPNOS_VERSION::V3_2, false);

// Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}

void setup() {

  // Enable the hypnos rails
  hypnos.enable();
  
  // Start and wait for the user to open the Serial monitor
  Serial.begin(115200);
  while(!Serial);
  
  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);
}

void loop() {

  // Set the RTC interrupt alarm to wake the device in 10 seconds
  hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 10));

  // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  hypnos.reattachRTCInterrupt();
  
  // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep(true);
}
