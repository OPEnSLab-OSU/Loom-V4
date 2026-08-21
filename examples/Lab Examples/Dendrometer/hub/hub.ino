#include "arduino_secrets.h"

#include <Loom_Manager.h> //4.7
#include <Diagnostics/Loom_MemoryDiagnostics.h>

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
Loom_MemoryDiagnostics memoryDiagnostics;


int packetNumber = 0;
void setup()
{

  /* Enables logging logs to the SD card for later viewing under the 'debug' folder */
  ENABLE_SD_LOGGING;   
  
  // Function summaries cause extra SD open/write/close traffic on every instrumented call.
  // Leave them disabled during endurance deployments; the bounded [MEM] checkpoints remain on.
    // Start the serial interface
    manager.beginSerial();
    memoryDiagnostics.checkpoint(F("post_global_ctor"), manager.getDocument(), -1);

    // Enable the power rails on the hypnos
    hypnos.enable();

    setRTC();

    // Sets the LTE board to use batch SD to only start when we actually need to publish data



    // load MQTT credentials from the SD card, if they exist
    memoryDiagnostics.checkpoint(F("pre_mqtt_config"), manager.getDocument(), -1);
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
    memoryDiagnostics.checkpoint(F("post_mqtt_config"), manager.getDocument(), -1);

    // Initialize the modules
    memoryDiagnostics.checkpoint(F("pre_initialize"), manager.getDocument(), -1);
    manager.initialize();
    memoryDiagnostics.checkpoint(F("setup_done"), manager.getDocument(), -1);
}

void loop()
{
    memoryDiagnostics.beginCycle();
    memoryDiagnostics.checkpoint(F("pre_lora_receive"), manager.getDocument(), -1);
    // Wait 5 seconds for a message
    if (lora.receive(5000, true))
    {
        memoryDiagnostics.checkpoint(F("post_lora_receive"), manager.getDocument(), -1);
        manager.display_data();
        memoryDiagnostics.checkpoint(F("pre_sd"), manager.getDocument(), -1);
        hypnos.logToSD();
        memoryDiagnostics.checkpoint(F("post_sd"), manager.getDocument(), -1);
        memoryDiagnostics.checkpoint(F("pre_mqtt"), manager.getDocument(), -1);
        mqtt.publish();
        memoryDiagnostics.checkpoint(F("post_mqtt"), manager.getDocument(), -1);
    }
  static unsigned long timer = millis();
  if (millis() - timer > REPORT_INTERVAL)
      {
          // manager.set_device_name("Hub");
          // manager.set_instance_num(0);

          manager.measure();
          manager.package();
          memoryDiagnostics.checkpoint(F("heartbeat_packaged"), manager.getDocument(), -1);
          manager.display_data();
          mqtt.publish();
          memoryDiagnostics.checkpoint(F("heartbeat_published"), manager.getDocument(), -1);
          
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
