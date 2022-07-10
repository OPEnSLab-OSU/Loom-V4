/**
 * This is an lab example for an LTE Dendrometer hub
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MQTT/Loom_MQTT.h>

Manager manager("Hub", 1);

// Do we want to use the instance number as the LoRa address
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
Loom_LoRa lora(manager);
Loom_LTE lte(manager, "hologram", "", "", A5);
Loom_MQTT mqtt(manager, lte.getClient(), SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS);

void setup(){
    // Start the serial interface
    manager.beginSerial();

    // Enable the power rails on the hypnos
    hypnos.enable();

    // Initialize the modules
    manager.initialize();
}

void loop()
{

    // Wait 5 seconds for a message
    if (lora.receive(5000))
    {
        manager.display_data();
        hypnos.logToSD();
        mqtt.publish();
    }
}