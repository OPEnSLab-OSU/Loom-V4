/**
 * This is an example use case for Loomified wifi
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>
#include <Loom_Wifi.h>

Manager manager("Device", 1);

Loom_WIFI wifi(manager, SECRET_SSID, SECRET_PASS);

void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {
  wifi.verifyConnection();
  delay(5000);
}