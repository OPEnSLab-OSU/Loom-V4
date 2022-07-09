/**
 * This is a lab example combine LoRa communication and 4G connectivity
 */

#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Internet/Logging/Loom_MQTT/Loom_MQTT.h>

Manager manager("Device", 1);

// Do we want to use the instance number as the LoRa address
Loom_LoRa loRa(manager);
Loom_LTE lte(manager, NETWORK_NAME, NETWORK_USER, NETWORK_PASS);
Loom_MQTT mqtt(manager, lte.getClient(), SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS);

void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {

  // Wait 5 seconds for a message
  if(loRa.receive(5000)){

    // If a message was received display the JSON document and transmit it over MQTT
    manager.display_data();
    mqtt.publish();
  }
  
  // Wait 5 seconds
  manager.pause(5000);
}