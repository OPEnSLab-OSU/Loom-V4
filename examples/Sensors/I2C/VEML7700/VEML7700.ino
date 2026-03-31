/**
 * VEML7700 Example code to measure light luminance in Lux
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

 #include <Loom_Manager.h>

 #include <Sensors/I2C/Loom_VEML7700/Loom_VEML7700.h>

 Manager manager("Device", 1);

 Loom_VEML7700 veml(manager);

 void setup() {
    // Manager begins serial communication at 115200 baud
    manager.beginSerial();

    // Initialize VEML7700
    manager.initialize();
 }

 void loop() {
    // Measure Lux
    manager.measure();

    // Package data into JSON doc
    manager.package();

    // Print JSON doc in serial monitor
    manager.display_data();

    // Wait 5 sec
    manager.pause(5000);
 }