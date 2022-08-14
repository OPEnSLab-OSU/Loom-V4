/**
 * This is an example use case for Loomified wifi
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>

Manager manager("Device", 1);

Loom_WIFI wifi(manager, SECRET_SSID, SECRET_PASS);

void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {
  wifi.verifyConnection();
  manager.pause(5000);
}