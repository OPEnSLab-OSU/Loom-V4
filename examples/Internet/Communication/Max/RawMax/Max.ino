/**
 * This is an example use case for Loomified wifi
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>
#include <Loom_Wifi.h>
#include <Loom_Max.h>

Manager manager("Device", 1);

Loom_WIFI wifi(manager, SECRET_SSID, SECRET_PASS);
Loom_Max maxMsp(manager, wifi);


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
  delay(5000);
}
