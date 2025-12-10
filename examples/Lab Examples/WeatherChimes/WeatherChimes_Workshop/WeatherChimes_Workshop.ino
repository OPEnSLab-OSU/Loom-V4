/**
 * In lab use case example for the WeatherChimes project
 * 
 * This project uses SDI12, TSL2591 and an SHT31 sensor to log environment data and logs it to both the SD card and also MQTT/MongoDB
 * 
 * Note: Every packet should have an RSSI value of 99 because the LTE board is powered down and not connected. This is okay and intended.
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

//  Updated December 10, 2025 - EZ


#include <Loom_Manager.h>
#include <Logger.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Sensors/I2C/Loom_TSL2591/Loom_TSL2591.h>
#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>
#include <Hardware/Loom_TippingBucket/Loom_TippingBucket.h>
#include <Sensors/Analog/Loom_Teros10/Loom_Teros10.h>

/* ---------------------------------- Hi, this is where we can edit code for each device specifically ---------------------------------------- */

// give the device a name and instance number
Manager manager("Chime_Name", 1);

// use true if the device should use 4G LTE and false if it should not
#define USE_LTE true

/* ---------------------------------------- nothing beyond this point should need to change --------------------------------------------------- */



// Pin to have the secondary interrupt triggered from
#define INT_PIN A0


// Used to track timing for debounce
unsigned long tip_time = 0;
unsigned long last_tip_time = 0;

int cycleCounter = 60;
bool is_first_upload = true;

TimeSpan sleepInterval;

// Create a new Hypnos object
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

// Analog for reading battery voltage
Loom_Analog analog(manager);

// Create sensor classes
Loom_SHT31 sht(manager);
Loom_TSL2591 tsl(manager);
Loom_MS5803 ms_water(manager, 119); 
Loom_MS5803 ms_air(manager, 118); 
Loom_TippingBucket bucket(manager, COUNTER_TYPE::MANUAL, 0.01f);
Loom_Teros10 teros(manager, A1);

#if USE_LTE
  #include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
  #include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
  Loom_LTE lte(manager, "hologram","","");
  Loom_MongoDB mqtt(manager, lte);
#endif
  
/* Calculate the water height based on the difference of pressures*/
float calculateWaterHeight(){
  // ((Water Pressure - Air Pressure) * 100 (conversion to pascals)) / (Water Density * Gravity)
  return (((ms_water.getPressure()-ms_air.getPressure()) * 100) / (997.77 * 9.81));
}

void tipTrigger() {
  hypnos.shouldPowerUp = false;
  bucket.incrementCount();
  LOG("tipping bucket incramented");
}

void setup() {

  manager.beginSerial();
  

  #if USE_LTE
    hypnos.setNetworkInterface(&lte);
  #endif

  // Enable the hypnos rails
  hypnos.enable();

  // get config from SD
  hypnos.getConfigFromSD("SD_config.json");

  // Give the bucket an instance of the hypnos
  bucket.setHypnosInstance(hypnos);

  #if USE_LTE
    // Read the MQTT creds file to supply the device with MQTT credentials
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
  #endif
  // Initialize all in-use modules 
  manager.initialize();

  attachInterrupt(INT_PIN, tipTrigger, FALLING);
  attachInterrupt(INT_PIN, tipTrigger, FALLING);
}

void loop() {

  // Measure and package the data
  manager.measure();
  manager.package();

  // Add the water height calculation to the data
  manager.addData("Water", "Height_(m)", calculateWaterHeight());
  
  // Print the current JSON packet
  manager.display_data();            

  // Log the data to the SD card              
  hypnos.logToSD();

  #if USE_LTE
    // Publish the collected data to MQTT

    if (cycleCounter == 60){

      // lte.restartModem();
      // lte.connect();

      if (is_first_upload){
        is_first_upload = false;
      }else{
        manager.power_up();
      }

      mqtt.publish();
      cycleCounter = 0;

      manager.power_down();


    }

  #endif

  // Reattach to the interrupt after we have set the alarm so we can have repeat triggers
  attachInterrupt(INT_PIN, tipTrigger, FALLING);
  attachInterrupt(INT_PIN, tipTrigger, FALLING);

  manager.pause(5000);
  cycleCounter++;
  

}