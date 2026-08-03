/**
 * Loom_DFRobotO2 Example code
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */


 #include <Loom_Manager.h>

 #include <Sensors/I2C/Loom_DFRobotO2/Loom_DFRobotO2.h>
 
 Manager manager("Device", 1);
 
 // Initialize the DFRobotO2 Sensor
 Loom_DFRobotO2 oxygen(manager);
 
 void setup() {
 
   // Start the serial interface
   manager.beginSerial();
 
   // Initialize the manager
   manager.initialize();

   /**
   * Choose method 1 or method 2 or method 3 to calibrate the oxygen sensor.
   * 1. Directly calibrate the oxygen sensor by adding two parameters to the sensor.
   * 2. Waiting for stable oxygen sensors for about 10 minutes, 
   *    OXYGEN_CONECTRATION is the current concentration of oxygen in the air (20.9% by volume except in special cases),
   *    Note using the first calibration method, the OXYGEN MV must be 0.
   * 3. Click the button on the back of the Gravity breakout board when in air after the value stabilizes, and it
   *    will calibrate the current concentration reading to be 20.9% by volume
   * 
   * oxygen.calibrate(calOxygenConcentration, calOxygenMV);
   * @param calOxygenConcentration oxygen concentration unit vol
   * @param calOxygenMV calibrated voltage unit mv   
   */ 
   //Uncomment the next line to calibrate the sensor
   //oxygen.calibrate(20.9, 0.0);

 }
 
 void loop() {
   // put your main code here, to run repeatedly:
 
   // Measure the data from the sensors
   manager.measure();
 
   // Package the data into JSON
   manager.package();
 
   // Print the JSON document to the Serial monitor
   manager.display_data();
 
   // Wait for 5 seconds
   manager.pause(5000);
 }
 
