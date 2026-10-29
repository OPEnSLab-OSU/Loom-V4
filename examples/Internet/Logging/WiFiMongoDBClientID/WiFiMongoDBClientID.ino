/**
 * This is an example use case for Loomified wifi and MQTT to log data remotely with a fixed MQTT client ID
 *
 * By default the client ID is the device name and instance number (here "Device1"), so the device is identifiable in the broker log.
 * Call setClientID() only to use a different ID.
 * The ID must be unique per device: the broker disconnects an existing client when another connects with the same ID.
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

  // Optional override of the default "Device1"; must be set before the first publish connects to the broker
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
