/**
 * This is an lab example for a Dendrometer node
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Hardware/Actuators/Loom_Neopixel/Loom_Neopixel.h>

#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>

#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MQTT/Loom_MQTT.h>

Manager manager("Dendro_LB_", 3);

// Do we want to use the instance number as the LoRa address
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

Loom_Analog analog(manager);
Loom_SHT31 sht(manager);
Loom_Neopixel neo(manager);

// Address: 3, Retry Count: 7, Timeout: 500
Loom_LoRa lora(manager, 3, 23, 7, 500);

void setup(){
    // Start the serial interface
    manager.beginSerial();

    // Set the base SD card file name
    hypnos.setLogName("Dend");

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