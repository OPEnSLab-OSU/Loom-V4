/**
 * This is an example use case for Loomified wifi and MQTT to log data remotely 
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Logging/Loom_MQTT/Loom_MQTT.h>

Manager manager("Device", 1);

Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS);
Loom_MQTT mqtt(manager, wifi.getClient(), SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS, PROJECT);

void setup() {
  manager.beginSerial();
  manager.initialize();
}

void loop() {
  manager.package();

  mqtt.publish();

  manager.pause(5000);
}