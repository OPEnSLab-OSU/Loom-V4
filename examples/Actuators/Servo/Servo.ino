/**
 * Turns a servo back and forth every second
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Hardware/Actuators/Loom_Servo/Loom_Servo.h>

Manager manager("Device", 1);

// Servo instance
Loom_Servo servo(manager, 0);

void setup() {

  // Start the serial interface and wait for the user to open the serial monitor
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();

}

void loop() {
  servo.setDegrees(180);
  manager.pause(1000);
  servo.setDegrees(0);
  manager.pause(1000);
}