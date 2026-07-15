/**
 * Author: Chet Udell
 * ENGR 102 Example Code: 
 * Read the Adafruit STEMMA i2c Soil Moisutre Sensor, 
 * Log everything to SD,
 * Turn on LED if soil is too dry and needs to be watered
 * Threshold value determined by a global variable
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 * Hypnos, Stemma are registered with the Manager
 * Setup section begins serial, enabled Hypnos library features, starts the Manager
 * Loop: Manager measures, packages, displays data. Hypnos used to log data to SD.
 * An if statement is used to evaluate the sensor data against a threshold and turns an LED on or off depending on the value
 * Manager.pause delays the processor to pause for 5 seconds and repeats loop from top
 */


#include <Loom_Manager.h>
#include <Logger.h>

#include <Sensors/I2C/Loom_STEMMA/Loom_STEMMA.h>

int threshold = 1000; // Define the threshold for what sensor value is too dry

Manager manager("YourName", 1); // Change the contents inside of the quotation marks to the name you want to call the device.
                                // DO NOT USE Spaces! Use_underscores, or useCamelNotation

// Create a new Hypnos object setting the version to determine the SD Chip select pin, and starting without the SD card functionality
//Loom_Hypnos(Manager& man, HYPNOS_VERSION version, TIME_ZONE zone, bool use_custom_time = false, bool useSD = true)
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::ADALOGGER, TIME_ZONE::PST, true);

// Register i2c STEMMA soil sensor with manager. Creates an instance called "stemma"
Loom_STEMMA stemma(manager);

void setup() {

  // Start the serial interface
  manager.beginSerial();

  // Enable the hypnos rails
  hypnos.enable();
      // This will log all LOG, ERROR, WARNING and any other logging calls to a file on the SD card
    ENABLE_SD_LOGGING;
    // Enables debug memory usage function summaries that will be logged to the SD card
    ENABLE_FUNC_SUMMARIES;

  // Initialize the manager
  manager.initialize();
}

void loop() {
  // put your main code here, to run repeatedly:

  // Measure the data from the sensors
  manager.measure();

  // Package the data into JSON
  manager.package();

  // Print the JSON document to the Serial monitor
  manager.display_data();

  // Log to the SD card twice and then lay dormant
  hypnos.logToSD();

  // Turn on LED if soil too dry
  if(stemma.getCapacitive() < threshold) // Is the soil moisture sensor value LESS THAN the threshold "global variable" we declared above?
    digitalWrite(LED_BUILTIN, HIGH);  // If True: turn the LED on 
  else
    digitalWrite(LED_BUILTIN, LOW);  // If False: turn the LED off

  // Wait for 5 seconds
  manager.pause(5000);
}
