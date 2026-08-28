/**
 * Example for the DF Robot Multi Gas Sensor
 */

#include <Loom_Manager.h>

#include <Sensors/I2C/Loom_DFMultiGasSensor/Loom_DFMultiGasSensor.h>

#include <Hardware/Loom_Multiplexer/Loom_Multiplexer.h>


Manager manager("Device", 1);   // If the sensor is freezing on init try disconnecting the power and re-connecting it

// MANAGER, I2C ADDRESS, INIT RETRY LIMIT, SENSOR POWER-CYCLES, USE MUX
Loom_DFMultiGasSensor gas(manager, 0x77, 10, false, false);

//Loom_Multiplexer mux(manager, {0x74});    // If using Multiplexer, use base addr 0x74


void setup() 
{
  manager.beginSerial();
  manager.initialize();
}


void loop() 
{
  manager.measure();
  manager.package();
  manager.display_data();
  manager.pause(5000);
}
