#include "arduino_secrets.h"

#include <Loom_Manager.h>

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
Loom_MongoDB mqtt(manager, lte.getClient(), SECRET_BROKER, SECRET_PORT, DATABASE, BROKER_USER, BROKER_PASS);

void setup()
{
    // Start the serial interface
    manager.beginSerial();

    // Enable the power rails on the hypnos
    hypnos.enable();

    setRTC();

    // load MQTT credentials from the SD card, if they exist
    mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));

    // Initialize the modules
    manager.initialize();
}

void loop()
{
    // Wait 5 seconds for a message
    if (lora.receive(5000))
    {
        manager.display_data();
        hypnos.logToSD();
        mqtt.publish();
    }

    // Send error packet to MongoDB under Hub folder
    else {
        manager.set_device_name("Hub");
        manager.set_instance_num(0);

        // // Clear manager JSON doc
        manager.getDocument().clear();

        // Manually construct error message
        manager.getDocument()[F("type")] = F("error");
        manager.getDocument()["id"]["name"] = "Hub";
        manager.getDocument()["id"]["instance"] = 0;

        // Get timestamp from RTC, format it
        DateTime now = hypnos.getCurrentTime();
        DateTime local = hypnos.getLocalTime(now);
    
        char utc[20];
        snprintf(utc, sizeof(utc), "%04d-%02d-%02dT%02d:%02d:%02dZ",
            now.year(), now.month(), now.day(), now.hour(), now.minute(), now.second());
        
        char local_time[20];
        snprintf(local_time, sizeof(local_time), "%04d-%02d-%02dT%02d:%02d:%02dZ",
            local.year(), local.month(), local.day(), local.hour(), local.minute(), local.second());

        manager.getDocument()["time_utc"] = utc;
        manager.getDocument()["time_local"] = local_time;
        manager.getDocument()["Message"] = "Packet receiving failure in transmission";

        // Send to MongoDB
        mqtt.publish();
    }

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