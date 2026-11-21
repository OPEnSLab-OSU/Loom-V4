/**
 * This is a lab example for Evaporometer
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/SPI/Loom_ADS1232/Loom_ADS1232.h>

Manager manager("Device", 1);

// Select the Hypnos version for its SD chip select pin.
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
Loom_ADS1232 ads(manager);

// Called when the interrupt is triggered
void isrTrigger(){
  hypnos.wakeup();
}

void setup(){

  manager.beginSerial(false);
  hypnos.setCompileTime(__DATE__, __TIME__);

  // Enable the hypnos rails
  hypnos.enable();

  // Initialize the manager
  manager.initialize();

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);
}

void loop(){

  if (!hypnos.setSampleInterval(TimeSpan(30))) {
    manager.pause(30000);
    hypnos.enable();
    return;
  }

  // Measure and package the data from the sensors
  manager.measure();
  manager.package();

  if (!hypnos.logToSD()) {
    Serial.println(F("Could not confirm the Evaporometer sample was saved to SD."));
  }
  hypnos.sleep();
}
