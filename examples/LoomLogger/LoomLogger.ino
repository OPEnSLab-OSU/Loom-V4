/**
 * Example code showing the LoomLogger
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>
#include <Logger.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

Manager manager("Device", 1);

// The hypnos is needed because it is how the SD is controlled
// Create a new Hypnos object setting the version to determine the SD Chip select pin
//Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time = false, bool useSD = true)
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);


void setup() {
    // Start the serial interface and wait for the user to open the serial monitor
    manager.beginSerial();

    // This enables the power rails on the hypnos
    hypnos.enable();

    // This will log all LOG, ERROR, WARNING and any other logging calls to a file on the SD card
    ENABLE_SD_LOGGING;

    // Enables debug memory usage function summaries that will be logged to the SD card
    ENABLE_FUNC_SUMMARIES;

    // Initialize the manager
    manager.initialize();

}

void loop() {
    
    // Example log
    LOG("Measure the sensors!");

    // Measure the data from the sensors
    manager.measure();

    // Example warning
    WARNING("Random waring!!");

    // Package the data into JSON
    manager.package();

    // Print the JSON document to the Serial monitor and Display it on the OLED
    manager.display_data();

    // Example error
    ERROR("Oh noooo");

    // Wait for 2 seconds
    manager.pause(2000);

}