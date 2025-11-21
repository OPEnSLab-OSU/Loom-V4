#include <Loom_Manager.h>
#include <Logger.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/I2C/Loom_MMA8451/Loom_MMA8451.h>
#include "arduino_secrets.h"

// Loom
Manager manager("walking_skeleton", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

// Connectivity
Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS);
Loom_MongoDB mqtt(manager, wifi, SECRET_BROKER, SECRET_PORT, DB_NAME, BROKER_USER, BROKER_PASS);

// Sensors
Loom_Analog analog(manager);
Loom_MMA8451 mma(manager);

void setup() {
  // Wait for user to open serial monitor
  manager.beginSerial(true);

  // Initialization
  hypnos.setNetworkInterface(&wifi);
  hypnos.enable();
  manager.initialize();
  hypnos.networkTimeUpdate();

  // Wait before reading sensors
  manager.pause(2000);
}

// Every 2 seconds take a sensor reading and publish to MongoDB
void loop() {
  // Measure and package the data
  manager.measure();
  manager.package();

  // Print the current JSON packet
  manager.display_data();            

  // Log the data to the SD card              
  hypnos.logToSD();

  // Publish the collected data to MQTT
  mqtt.publish();

  // Wait before next sensor reading
  manager.pause(2000);
}
