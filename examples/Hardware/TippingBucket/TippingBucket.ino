/**
 * Tipping Bucket example code showing how to count tips from a rainfall tipping bucket
 * A hypnos can be used to calculate rainfall accumulated over the last hour
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


#include <Loom_Manager.h>

#include <Hardware/Loom_TippingBucket/Loom_TippingBucket.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

Manager manager("Device", 1);

// We want this or we have to use 10k pullups on the I2C line
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

// (This one uses a I2C counter on the PCB) Manger Instance, Inches of rainfall per tip
// Loom_TippingBucket bucket(manager, 0.01f);

// (This one uses an interrupt) Manger Instance, Interrupt Pin, Inches of rainfall per tip (WIP)
Loom_TippingBucket bucket(manager, COUNTER_TYPE::MANUAL, 0.01f);

// Pin to have the secondary interrupt triggered from
#define INT_PIN A0

volatile bool tipFlag = false;

void tipTrigger() {
  hypnos.shouldPowerUp = false;
  tipFlag = true;
  detachInterrupt(INT_PIN);
}


void setup() {

  // Start the serial interface and wait for the user to open the serial monitor
  manager.beginSerial();

  // If using the hypnos we need to enable it before we continue 
  hypnos.enable();

  // Set an instance of the hypnos inside the tipping bucket object so we can reference the current time
  bucket.setHypnosInstance(hypnos);

  // Initialize the manager
  manager.initialize();

  attachInterrupt(INT_PIN, tipTrigger, FALLING);
}

void loop() {
  // Measure the data from the sensors
  manager.measure();

  // Package the data into JSON
  manager.package();

  // Print the collected output to the Serial monitor
  manager.display_data();

  // Wait for 2 seconds
  manager.pause(500);

  // Log tip if the flag was set to true through an interrupt
  if(tipFlag){
    digitalWrite(LED_BUILTIN, LOW);
    delay(50);
    bucket.incrementCount();
    tipFlag = false;
    attachInterrupt(INT_PIN, tipTrigger, FALLING);
    attachInterrupt(INT_PIN, tipTrigger, FALLING);
    digitalWrite(LED_BUILTIN, HIGH);
  }

}
