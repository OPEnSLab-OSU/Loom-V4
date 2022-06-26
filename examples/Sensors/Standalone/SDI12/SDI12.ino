/**
 * This is an example use case for the SDI-12 implementation without the use of a manger to allow for maximum modularity
 * A Loomified version can be found in the "Loomified" section of the examples
 * 
 * This assumes a Hypnos board is in use as connection point if not you can remove the lines
 *  - pinMode(5, OUTPUT);
 *  - pinMode(6, OUTPUT);
 *  
 * This example scans the address space for devices and the requests data from the first device it finds
 */
#include <vector>
#include <Loom_SDI12.h>

Loom_SDI12 decagon(11);

void setup() {
  // Needs to be done for Hypnos Board
  pinMode(5, OUTPUT);       // Enable control of 3.3V rail
  pinMode(6, OUTPUT);       // Enable control of 5V rail

  Serial.begin(115200);
  while(!Serial);

  decagon.initialize();     // Initialize the decagon sensor
  
}

void loop() {

  std::vector<float> data = decagon.getData(decagon.getInUseAddresses()[0]);
  Serial.println("Dielectric Perm: " + String(data[0]));
  Serial.println("Temp: " + String(data[1]));
  Serial.println("Conductivity: " + String(data[2]));

  delay(4000);
}