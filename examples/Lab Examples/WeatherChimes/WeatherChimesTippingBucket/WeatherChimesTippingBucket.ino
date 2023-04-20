/**
 * In lab use case example for the WeatherChimes project
 * 
 * This project uses SDI12, TSL2591 and an SHT31 sensor to log environment data and logs it to both the SD card and also MQTT/MongoDB
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>
#include <Logger.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Sensors/I2C/Loom_TSL2591/Loom_TSL2591.h>
#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>

#include <Internet/Logging/Loom_MQTT/Loom_MQTT.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>


// Pin to have the secondary interrupt triggered from
#define INT_PIN A0

volatile bool sampleFlag = true; // Sample flag set to 1 so we sample in the first cycle, set to 1 in the ISR, set ot 0 end of sample loop
volatile bool tipFlag = false;
volatile int counter = 0;

// Used to track timing for debounce
unsigned long tip_time = 0;
unsigned long last_tip_time = 0;

Manager manager("Chime", 11);

// Create a new Hypnos object
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

// Analog for reading battery voltage
Loom_Analog analog(manager);

// Create sensor classes
Loom_SHT31 sht(manager);
Loom_TSL2591 tsl(manager);
Loom_MS5803 ms_water(manager, 119); // 119(0x77) if CSB=LOW external, 118(0x76) if CSB=HIGH on WC PCB
Loom_MS5803 ms_air(manager, 118); // 118(0x76) if CSB=HIGH on WC PCB


Loom_LTE lte(manager, "hologram", "", "");
Loom_MQTT mqtt(manager, lte.getClient());

/* Calculate the water height based on the difference of pressures*/
float calculateWaterHeight(){
  // ((Water Pressure - Air Pressure) * 100 (conversion to pascals)) / (Water Density * Gravity)
  return (((ms_water.getPressure()-ms_air.getPressure()) * 100) / (997.77 * 9.81));
}

// Called when the interrupt is triggered 
void isrTrigger(){
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

  // Enable debug SD logging and function summaires
  ENABLE_SD_LOGGING;
  ENABLE_FUNC_SUMMARIES;

  // Set the interrupt pin to pullup
  pinMode(INT_PIN, INPUT_PULLUP);

  // Wait 20 seconds for the serial console to open
  manager.beginSerial();

  // Enable the hypnos rails
  hypnos.enable();

  // Read the MQTT creds file to supply the device with MQTT credentials
  mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));

  // Initialize all in-use modules
  manager.initialize();

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);

  attachInterrupt(INT_PIN, tipTrigger, FALLING);
}

void loop() {

  if(sampleFlag){
    // Measure and package the data
    manager.measure();
    manager.package();

    manager.addData("Tip_Bucket", "Tip_Count", counter);

    // Add the water height calculation to the data
    manager.addData("Water", "Height_(m)", calculateWaterHeight());
    
    // Print the current JSON packet
    manager.display_data();            

    // Log the data to the SD card              
    hypnos.logToSD();

    // Publish the collected data to MQTT
    mqtt.publish();

    // Set the RTC interrupt alarm to wake the device in 15 min
    hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 15));

    // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
    hypnos.reattachRTCInterrupt();
    sampleFlag = false;
  }

  if(tipFlag){
    tipFlag = false;
    attachInterrupt(INT_PIN, tipTrigger, FALLING);
    attachInterrupt(INT_PIN, tipTrigger, FALLING);
  }
  
  // Put the device into a deep sleep, operation HALTS here until the interrupt is triggered
  hypnos.sleep();
  
}
