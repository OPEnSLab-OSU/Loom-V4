#include <Loom_Manager.h>
#include <Logger.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#include <Sensors/Loom_Analog/Loom_Analog.h>

#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
// #include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>

Manager manager("Device", 1);

// Create a new Hypnos object
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

// Analog for reading battery voltage
Loom_Analog analog(manager);

Loom_LTE lte(manager, "hologram","","");
// Loom_WIFI wifi(manager, CommunicationMode::CLIENT, "Kuti", "kuti101!");

Loom_MongoDB mqtt(manager, lte);

void setup() {

  // Enable debug SD logging and function summaries
  ENABLE_SD_LOGGING;
  ENABLE_FUNC_SUMMARIES;

  // Wait 20 seconds for the serial console to open
  manager.beginSerial();

  hypnos.setNetworkInterface(&lte);

  // Enable the hypnos rails
  hypnos.enable();

  // Read the MQTT creds file to supply the device with MQTT credentials
  mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));

  // Initialize all in-use modules
  manager.initialize();

  mqtt.publishMetadata(hypnos.readFile("metadata.json")); //Publish the metadata.json file from the SD card during initialization
}

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

  manager.pause(5000);
}
