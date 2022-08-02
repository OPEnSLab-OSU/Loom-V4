/**
 * Stepper example showing manual control of the motor
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Hardware/Actuators/Loom_Stepper/Loom_Stepper.h>

Manager manager("Device", 1);

// Reads the battery voltage
Loom_Stepper stepper(manager, 0);

void setup() {

  // Start the serial interface and wait for the user to open the serial monitor
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();
}

void loop() {
    // Move 100 steps at 10 RPM moving clockwise
    stepper.moveSteps(100, 10);

    // Move 100 steps at 10 rpm moving counterclockwise
    stepper.moveSteps(100, 10, false);
}