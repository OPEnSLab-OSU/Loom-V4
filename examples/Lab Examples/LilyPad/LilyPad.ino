/**
 * LilyPad4G.ino
 * 
 * This project (https://github.com/OPEnSLab-OSU/Lilypad) uses the VEML7700, DS18B20, and SHT31 sensors to log environment data and logs it to both the SD card and also MQTT/MongoDB
 * Required Packages: Altered VEML7700, OneWire, DallasTemperature, Loom4 commit 63b8fd5 or later
 */

// COMMENT THE FOLLOWING LINE TO NOT USE INTERNET
#define USE_INTERNET
// COMMENT THE FOLLOWING LINE TO NOT USE RTC INTERRUPT
#define USE_RTC_INT
#define INT_MIN 2      // Sets sleep time in minutes when RTC is used
#define DEBUG_DELAY 2  // Sets delay time in seconds when rtc interrupt isnt used

#define COLLECTION_NAME "LP1"

#include "arduino_secrets.h"

// Loom includes
#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Logger.h>
// Sensor Includes
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
// Internet/DB Includes
#ifdef USE_INTERNET
#include <Internet/Logging/Loom_MQTT/Loom_MQTT.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#endif
// OneWire Includes
#include <OneWire.h>
#include <DallasTemperature.h>
// Adafruit VEML7700 breakout include
#include "Adafruit_VEML7700.h"

Manager manager(COLLECTION_NAME, 1);                                      //Establish the Loom manager first
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_2, TIME_ZONE::PST, true);  // Create a new Hypnos object
Loom_Analog analog(manager);                                              // Add battery voltage measurement

// Create sensor classes
Loom_SHT31 sht(manager);
// Create lte and mqtt classes
#ifdef USE_INTERNET
Loom_LTE lte(manager, NETWORK_APN, NETWORK_USER, NETWORK_PASS);
Loom_MQTT mqtt(manager, lte.getClient(), SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS, "LilyPad");
#endif

// Create oneWire/DallasTemperature sensor classes
#define ONE_WIRE_BUS 11
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
DeviceAddress ds18b20Address;
bool dsb18bInitialized = true;

// Initialize Adafruit VEML7700 class
Adafruit_VEML7700 veml = Adafruit_VEML7700();
bool vemlInitialized = true;

// Called when the RTC interrupt is triggered
void isrTrigger() {
  hypnos.wakeup();
}

void setup() {
  ENABLE_SD_LOGGING;
  ENABLE_FUNC_SUMMARIES;
  manager.beginSerial();                 // Wait 20 seconds for the serial console to open
  hypnos.enable();                       // Enable the hypnos rails
  manager.initialize();                  // Initialize all in-use modules
  hypnos.registerInterrupt(isrTrigger);  // Register the ISR and attach to the interrupt
  sensors.begin();                       // locate onewire devices on the bus
  Serial.printf("[OneWire] Found %d devides...\n", sensors.getDeviceCount());
  if (sensors.getDeviceCount() == 0) {
    Serial.println("[DS18B20] Sensor not found...");
    dsb18bInitialized = false;
  }else{
    if (!sensors.getAddress(ds18b20Address, 0)) Serial.println("[DS18B20] Unable to find address for Device 0");
    Serial.printf("[DS18B20] Address: 0x%x...\n", ds18b20Address);
    sensors.setResolution(ds18b20Address, 12);  // Set to the highest resolution
    Serial.printf("[DS18B20] Resolution: %d...\n", sensors.getResolution(ds18b20Address));
  }

  Wire.begin();
  if (!veml.begin()) {  // Startup veml sensor communication and set defaults
    Serial.println("[VEML7700] Sensor not found...");
    vemlInitialized = false;
  } else {
    Serial.println("[VEML7700] Sensor found...");
  }
}

void loop() {
  if(dsb18bInitialized){
    sensors.requestTemperatures();                   // Send the command to get temperatures
    float tempC = sensors.getTempC(ds18b20Address);  // Extract the temperature in C
  }
  else{
    Serial.println("[DS18B20] Sensor Not Initialized");
  }
  if(vemlInitialized){
    veml.begin();
    float autoLux = veml.readLux(VEML_LUX_AUTO);
    uint16_t rawALS = veml.readALS();
    uint16_t rawWhite = veml.readWhite();
  }
  else{
    Serial.println("[VEML7700] Sensor Not Initialized");
  }
  manager.measure();                                   // Measure sensor values
  manager.package();                                   // Package data from measurments

  if(dsb18bInitialized)
    manager.addData("DS18B20", "Temperature", tempC);    // Manually add the data to the JSON

  if(vemlInitialized){
    manager.addData("VEML7700", "Raw_ALS", rawALS);      // Manually add raw ALS value
    manager.addData("VEML7700", "Raw_White", rawWhite);  //Manually add raw White value
    manager.addData("VEML7700", "LUX", autoLux);         // Manually add auto LUX value
  }
  manager.display_data();                              // Print the current JSON packet
  hypnos.logToSD();
  if(vemlInitialized)
    veml.end();  // Log the data to the SD card
  #ifdef USE_INTERNET
  mqtt.publish();  // Publish the collected data to MQTT
  #endif
  #ifdef USE_RTC_INT
  hypnos.setInterruptDuration(TimeSpan(0, 0, INT_MIN, 0));  // Interrupt
  hypnos.reattachRTCInterrupt();
  hypnos.sleep();
  #else
  delay(DEBUG_DELAY * 1000);
#endif
}
