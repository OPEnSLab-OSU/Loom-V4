/**
 * This is an example use case for Loomified wifi and MQTT to log data remotely 
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>
#include <Loom_Wifi.h>
#include <Loom_MQTT.h>

Manager manager("Device", 1);

Loom_WIFI wifi(manager, SECRET_SSID, SECRET_PASS);
Loom_MQTT mqtt(manager, wifi.getClient(), SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS);

void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {
  manager.package();
  mqtt.publish();
  delay(5000);
}