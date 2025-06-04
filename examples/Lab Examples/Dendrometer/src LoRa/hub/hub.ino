#include "arduino_secrets.h"

#include <Loom_Manager.h> //4.7

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Radio/Loom_LoRa/Loom_LoRa.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

const unsigned long REPORT_INTERVAL = 1 * 60 * 60 * 1000;

Manager manager("Hub", 0);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
Loom_Analog batteryVoltage(manager);
Loom_LoRa lora(manager);
Loom_LTE lte(manager, "hologram", "", "", A5);
Loom_MongoDB mqtt(manager, lte, SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS);
Loom_BatchSD batchSD(hypnos, 16); //set batch size uploading

int packetNumber = 0;
void setup()
{
    // Start the serial interface
    manager.beginSerial();

    // Enable the power rails on the hypnos
    hypnos.enable();

    setRTC();

    // Sets the LTE board to use batch SD to only start when we actually need to publish data
    lte.setBatchSD(batchSD);


    // load MQTT credentials from the SD card, if they exist
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));

    // Initialize the modules
    manager.initialize();
}

void loop()
{
    // Wait 5 seconds for a message
    do{
      if (lora.receiveBatch(5000, &packetNumber))
      {
          manager.display_data();
          hypnos.logToSD();
          mqtt.publish(batchSD);
      }
    }while(packetNumber > 0);
  static unsigned long timer = millis();
  if (millis() - timer > REPORT_INTERVAL)
      {
          manager.set_device_name("Hub");
          manager.set_instance_num(0);

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