/**
 * VEML7700 Example code with Hypnos to measure light luminance in Lux
 * 
 * 1 min intervals - sleep, power up/down
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

 #include <Loom_Manager.h>

 #include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

 #include <Sensors/I2C/Loom_VEML7700/Loom_VEML7700.h>

 Manager manager("Device", 1);

 Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

 Loom_VEML7700 veml(manager);

 void isrTrigger(){
    hypnos.wakeup();
 }

 void setup() {
    // Manager begins serial communication at 115200 baud
    manager.beginSerial();

    // Enable hypnos power rails
    hypnos.enable();

    // Initialize VEML7700
    manager.initialize();

    // Register ISR to hypnos
    hypnos.registerInterrupt(isrTrigger);
 }

 void loop() {
    // Measure Lux
    manager.measure();

    // Package data into JSON doc
    manager.package();

    // Print JSON doc in serial monitor
    manager.display_data();

    // Log data to hypnos SD
    hypnos.logToSD();

    // Set RTC alarm
    hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 10));

    // Reattatch to interrupt after set alarm so no repeated triggers
    hypnos.reattachRTCInterrupt();

    // Put MCU to sleep until RTC alarm fires and wakes device up
    hypnos.sleep();

    //manager.initialize();

}