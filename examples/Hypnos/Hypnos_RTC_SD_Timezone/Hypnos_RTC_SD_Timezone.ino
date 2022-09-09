/**
 * This is an example use case for the Hypnos board's SD functionality to load a timezone from the SD card
 * 
 * NOTE: THIS EXAMPLE DOESN"T WAIT FOR SERIAL AFTER SLEEPING
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

Manager manager("Device", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin, and starting without the SD card functionality
//Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time = false, bool useSD = true)
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2, TIME_ZONE::PST);

// Called when the interrupt is triggered 
void isrTrigger(){
  hypnos.wakeup();
}

void setup() {

  // Start and wait for the user to open the Serial monitor
  Serial.begin(115200);
  while(!Serial);

  // Load the Timezone before we enable the hypnos
  hypnos.getTimeZoneFromSD("Timezone.json");

  // Enable the hypnos rails
  hypnos.enable();
}

void loop() {
  
  // Print the current JSON packet
  manager.display_data();            

  // Log the data to the SD card              
  hypnos.logToSD();
}