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
Loom_MongoDB mqtt(manager, lte);
Loom_BatchSD batchSD(hypnos, 15); //set batch size uploading; must match the node's TRANSMIT_INTERVAL

int packetNumber = 0;
void setup()
{
    // Write debug logs to the SD card (/debug/output_N.log)
    ENABLE_SD_LOGGING;

    // Start the serial interface
    manager.beginSerial();

    // Enable the power rails on the hypnos
    hypnos.enable();

    setRTC();

    // Sets the LTE board to use batch SD to only start when we actually need to publish data
    lte.setBatchSD(batchSD);


    // load MQTT credentials from the SD card, if they exist
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));

    // Set the client ID to something identifiable in the Mosquitto logs
    mqtt.setClientID("HazelnutDendrometerHub0_Test");

    // Initialize the modules
    manager.initialize();
}

void loop()
{
    // Wait 5 seconds for a message
    do{
      if (lora.receiveBatch(5000, &packetNumber))
      {
          // receiveBatch proxied the sender's identity, so the instance is the node number
          manager.addData("Node", "Number", manager.get_instance_num());
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