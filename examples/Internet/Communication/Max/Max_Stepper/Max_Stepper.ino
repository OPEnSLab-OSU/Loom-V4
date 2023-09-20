/**
 * This is an example use case for a Max controlled Stepper motor
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Communication/Loom_Max/Loom_Max.h>
#include <Hardware/Actuators/Loom_Stepper/Loom_Stepper.h>


Manager manager("Device", 1);

Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS);
Loom_Max maxMsp(manager, wifi, new Loom_Stepper(0));


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
