/**
 * This is an example use case for Max controlled Relay
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Publishing/Loom_Max.h>
#include <Hardware/Actuators/Loom_Relay/Loom_Relay.h>


Manager manager("Device", 1);

Loom_WIFI wifi(manager, SECRET_SSID, SECRET_PASS);
Loom_Max maxMsp(manager, wifi, new Loom_Relay());


void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {
  manager.package();
  manager.display_data();

  // Send and Recieve data from Max
  maxMsp.publish();
  maxMsp.subscribe();
  delay(1000);
}
