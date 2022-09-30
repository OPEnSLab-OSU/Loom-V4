// This example tests the MS5803
// Be sure to read the data sheet, esp with respect to the CSB pin 
// CBS pin low sets i2c address to 0x77 (119)
// CBS pin tied to VCC sets i2c address to 0x76 (118)
// Yes, addresses were double checked.

/**
 * MS5803 Example Code
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>

Manager manager("Device", 1);

// Manager Instance,      Address, useMux
Loom_MS5803 ms03(manager, 0x77, false); // MS5803 CSB pin tied to VCC i2c addr 0x76

void setup() {

  // Start the serial interface
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();
  
}

void loop() {
  // Put your main code here, to run repeatedly:

  // Measure and package the data from the sensors
  manager.measure();
  
  // Package the data into JSON
  manager.package();

  // Print the current JSON document to the Serial monitor
  manager.display_data();  

  // Wait for 2 seconds
  manager.pause(2000);  
  
}
