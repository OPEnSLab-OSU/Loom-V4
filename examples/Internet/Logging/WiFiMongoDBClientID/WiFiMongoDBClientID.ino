/**
 * This is an example use case for Loomified wifi and MQTT to log data remotely with a fixed MQTT client ID
 *
 * By default the client ID is random (Arduino-xxxxxxxx), so the device can't be identified in the broker log.
 * setClientID() gives it a fixed name instead. The ID must be unique per device: the broker disconnects
 * an existing client when another connects with the same ID.
 *
 * Example Mosquitto log, default ID vs. setClientID("Device1_Lab"):
 *   1793223625: New client connected from 203.0.113.10:33176 as Arduino-1bd668f1 (p4, c1, k60, u'username').
 *   1793223974: Client Arduino-1bd668f1 [203.0.113.10:33176] disconnected: exceeded timeout.
 *   1793224093: New client connected from 203.0.113.11:36259 as Device1_Lab (p4, c1, k60, u'username').
 *   1793224185: Client Device1_Lab [203.0.113.11:36259] disconnected: exceeded timeout.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

Manager manager("Device", 1);

Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS);
Loom_MongoDB mqtt(manager, wifi, SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS, PROJECT);

void setup() {
  manager.beginSerial();

  // Must be set before the first publish connects to the broker
  mqtt.setClientID("Device1_Lab");

  manager.initialize();
}

void loop() {
  manager.measure();
  manager.package();
  manager.display_data();

  mqtt.publish();

  manager.pause(5000);
}
