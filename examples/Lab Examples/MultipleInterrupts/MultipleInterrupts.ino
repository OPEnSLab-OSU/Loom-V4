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
  tip_time = millis();

  // Check if the time of the last tip is more than 250 ms to debounce the switch
  if(tip_time - last_tip_time > 250){
    counter++;
    tipFlag = true;
    detachInterrupt(INT_PIN);
  }
}

void setup() {

  // Set the interrupt pin to pullup
  pinMode(INT_PIN, INPUT_PULLUP);

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
  
  // If we are waking up normally then we should sample data
  if(sampleFlag) {
    // Measure and package the data
    manager.measure();
    manager.package();
    manager.addData("Switch", "Status", counter);
    
    // Print the current JSON packet
    manager.display_data();            

    // Log the data to the SD card              
    hypnos.logToSD();

    // Set the RTC interrupt alarm to wake the device every 30 seconds
    hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 30));

    // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
    hypnos.reattachRTCInterrupt();

    sampleFlag = false; // Do not do this process unless ISR sets the flag
  }

  // Check if Tipping Bucket event
  if(tipFlag) {
    tipFlag = false;
    Serial.println(counter);
    attachInterrupt(INT_PIN, tipTrigger, FALLING);
    attachInterrupt(INT_PIN, tipTrigger, FALLING);
  }
  
   // Put the device into a deep sleep
  hypnos.sleep();
}