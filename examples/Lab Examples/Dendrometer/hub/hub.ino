#if __has_include("arduino_secrets.h")
#include "arduino_secrets.h"
#endif

#ifndef SECRET_SSID
#define SECRET_SSID ""
#endif
#ifndef SECRET_PASS
#define SECRET_PASS ""
#endif
#ifndef NETWORK_APN
#define NETWORK_APN ""
#endif
#ifndef NETWORK_NAME
#define NETWORK_NAME ""
#endif
#ifndef NETWORK_USER
#define NETWORK_USER ""
#endif
#ifndef NETWORK_PASS
#define NETWORK_PASS ""
#endif
#ifndef SECRET_BROKER
#define SECRET_BROKER ""
#endif
#ifndef SECRET_PORT
#define SECRET_PORT 0
#endif
#ifndef DATABASE
#define DATABASE ""
#endif
#ifndef BROKER_USER
#define BROKER_USER ""
#endif
#ifndef BROKER_PASS
#define BROKER_PASS ""
#endif
#ifndef PROJECT
#define PROJECT ""
#endif
#ifndef CHANNEL_ID
#define CHANNEL_ID 0
#endif
#ifndef CLIENT_ID
#define CLIENT_ID ""
#endif

#include <Loom_Manager.h> //4.7

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

const unsigned long REPORT_INTERVAL = 1 * 60 * 60 * 1000;

Manager manager("HubName", 0);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
Loom_Analog batteryVoltage(manager);
Loom_LoRa lora(manager);
Loom_LTE lte(manager, "hologram", "", "", A5);
Loom_MongoDB mqtt(manager, lte, SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS);


int packetNumber = 0;
void setup()
{

  /* Enables logging logs to the SD card for later viewing under the 'debug' folder */
  ENABLE_SD_LOGGING;   
  
  /* Enables generation of function summaries */
  ENABLE_FUNC_SUMMARIES;
    // Start the serial interface
    manager.beginSerial();

    // Enable the power rails on the hypnos
    hypnos.enable();

    setRTC();

    // Sets the LTE board to use batch SD to only start when we actually need to publish data



    // load MQTT credentials from the SD card, if they exist
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));

    // Initialize the modules
    manager.initialize();
}

void loop()
{
    // Wait 5 seconds for a message
    if (lora.receive(5000, true))
    {
        manager.display_data();
        hypnos.logToSD();
        mqtt.publish();
    }
  static unsigned long timer = millis();
  if (millis() - timer > REPORT_INTERVAL)
      {
          // manager.set_device_name("Hub");
          // manager.set_instance_num(0);

          manager.measure();
          manager.package();
          manager.display_data();
          mqtt.publish();
          
          timer = millis();
      }
}


void setRTC()
{
    if (!Serial)
        return;

    Serial.println(F("Adjust RTC time? (y/n)"));
    unsigned long timer = millis();
    while (!Serial.available() && (millis() - timer) < 7000)
        ;
    if (!Serial.available())
        return;
    int val = Serial.read();
    delay(50);
    while (Serial.available())
        Serial.read(); // flush the input buffer to avoid invalid input to rtc function

    if (val == 'y')
    {
        hypnos.set_custom_time();
    }
}