/**
 * This is an example use case for Communication data to max
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Communication/Loom_Max.h>

Manager manager("Device", 1);

Loom_WIFI wifi(manager, CommunicationMode::AP);
Loom_Max maxMsp(manager, wifi);


void setup() {

  // Start serial interface
  manager.beginSerial();

  // Init modules
  manager.initialize();
}

void loop() {
  manager.package();
  manager.display_data();

  // Send and Recieve data from Max
  maxMsp.publish();
  maxMsp.subscribe();
  
  manager.pause(1000);
}
