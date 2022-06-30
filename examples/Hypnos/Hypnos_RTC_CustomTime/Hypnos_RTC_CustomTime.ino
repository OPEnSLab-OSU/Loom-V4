/**
 * This is an example use case for setting a custom time on the Hypnos RTC
 */

#include <Loom_Hypnos.h>
#include <Loom_Manager.h>

Manager manager("Chime", 1);

// Create a new Hypnos object setting the version to determine the SD Chip select pin
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2, TIME_ZONE::PST, true, false);


void setup() {

   // Start and wait for the user to open the Serial monitor
  Serial.begin(115200);
  while(!Serial);
  
  // Enable the hypnos rails
  hypnos.enable();
}

void loop() {
  manager.package();

  manager.display_data();

  delay(4000);
}