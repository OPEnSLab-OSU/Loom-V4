/**
 * MS5803 Example Code
 *
 * Be sure to read the data sheet, esp with respect to the CSB pin
 * CBS pin low sets i2c address to 0x77 (119)
 * CBS pin tied to VCC sets i2c address to 0x76 (118)
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>

Manager manager("Device", 1);

Loom_MS5803 ms_water(manager, 0x77, false); // MS5803 CSB pin tied to VCC i2c addr 0x77
Loom_MS5803 ms_air(manager, 0x76, false); // MS5803 CSB pin tied to VCC i2c addr 0x76


/* Calculate the water height based on the difference of pressures*/
float calculateWaterHeight(){
  // ((Water Pressure - Air Pressure) * 100 (conversion to pascals)) / (Water Density * Gravity)
  return (((ms_water.getPressure()-ms_air.getPressure()) * 100) / (997.77 * 9.81));
}

void setup() {

  // Start the serial interface
  manager.beginSerial();

  // Initialize the manager
  manager.initialize();
  
}

void loop() {
  // Measure and package the data from the sensors
  manager.measure();
  
  // Package the data into JSON
  manager.package();

  // Add the water height calculation to the data
  manager.addData("Water", "Height_(m)", calculateWaterHeight());

  // Print the current JSON document to the Serial monitor
  manager.display_data();  

  // Wait for 2 seconds
  manager.pause(2000);  
  
}
