/**
 * Toggles a relay on and off every second
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Hardware/Actuators/Loom_Relay/Loom_Relay.h>

Manager manager("Device", 1);

// Reads the battery voltage
Loom_Relay relay(manager, 10);

void setup() {

  // Start the serial interface and wait for the user to open the serial monitor
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();
}

void loop() {
    relay.setState(true);
    manager.pause(1000);
    relay.setState(false);
    manager.pause(1000);
}