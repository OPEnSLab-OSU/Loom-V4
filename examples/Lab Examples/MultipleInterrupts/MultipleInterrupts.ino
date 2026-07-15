/**
 * In lab use case example for the Multiple Interrupts while sleeping
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

// Pin to have the secondary interrupt triggered from
#define INT_PIN A0

volatile bool sampleFlag = true; // Sample flag set to 1 so we sample in the first cycle, set to 1 in the ISR, set ot 0 end of sample loop
volatile bool tipFlag = false;
volatile int counter = 0;

// Used to track timing for debounce
unsigned long tip_time = 0;
unsigned long last_tip_time = 0;

Manager manager("Device", 0);

// Create a new Hypnos object
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

// Called when wake up interrupt is triggered
void wakeTrigger(){
  sampleFlag = true;
  hypnos.wakeup();
}

void tipTrigger() {
  hypnos.shouldPowerUp = false;
  tipFlag = true;
  detachInterrupt(INT_PIN);
}

void setup() {

  // Set the interrupt pin to INPUT
  pinMode(INT_PIN, INPUT);

  // Wait 20 seconds for the serial console to open
  manager.beginSerial();

  // Enable the hypnos rails
  hypnos.enable();

  // Initialize all in-use modules
  manager.initialize();

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(wakeTrigger);
  
  attachInterrupt(INT_PIN, tipTrigger, FALLING);
}

void loop() {
  
  if(sampleFlag){
    // Set the RTC interrupt alarm to wake the device in 15 min, this should be done as soon as the device enters sampling mode for consistant sleep cycles
    hypnos.setInterruptDuration(TimeSpan(0, 0, 15, 0));

    // Measure and package the data
    manager.measure();
    manager.package();
    
    // Print the current JSON packet
    manager.display_data();            

    // Log the data to the SD card              
    hypnos.logToSD();

    // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
    hypnos.reattachRTCInterrupt();
    attachInterrupt(INT_PIN, tipTrigger, FALLING);
    attachInterrupt(INT_PIN, tipTrigger, FALLING);
    sampleFlag = false;
  }

  if(tipFlag){
    digitalWrite(LED_BUILTIN, HIGH);
    delay(20);
    bucket.incrementCount();
    tipFlag = false;
    attachInterrupt(INT_PIN, tipTrigger, FALLING);
    attachInterrupt(INT_PIN, tipTrigger, FALLING);
    digitalWrite(LED_BUILTIN, LOW);
  }
  
  // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep();
}