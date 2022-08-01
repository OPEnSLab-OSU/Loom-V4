/**
 * Neopixel example code showing writing colors to the LED's
 * Cycles through Green, Yellow and Red changing every second
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Hardware/Actuators/Loom_Neopixel/Loom_Neopixel.h>

Manager manager("Device", 1);

// Reads the battery voltage
Loom_Neopixel neo(manager);

void setup() {

  // Start the serial interface and wait for the user to open the serial monitor
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();

  // Turns off Neopixel
  neo.set_color(2, 0, 0, 0, 0); 
}

void loop() {
    neo.set_color(2, 0, 200, 0, 0);   // Green
    manager.pause(1000);
    neo.set_color(2, 0, 200, 200, 0); // Yellow
    manager.pause(1000);
    neo.set_color(2, 0, 0, 200, 0);   // Red
    manager.pause(1000);
}