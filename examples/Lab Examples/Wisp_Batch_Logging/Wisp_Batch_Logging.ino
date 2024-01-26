#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/I2C/Loom_SEN55/Loom_SEN55.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>

//#include <Sensors/Analog/ACS712/Loom_ACS712.h>

#include <Logger.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

Manager manager("Whisp_Proto", 1);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

Loom_Analog analog(manager);

//Main Air Quality, Temperature, and Humidity Sensing
Loom_SEN55 SEN55(manager);
Loom_SHT31 sht(manager);

//Connectivity
Loom_LTE lte(manager, "hologram", "", "");
Loom_MongoDB mqtt(manager, lte.getClient());
//A batch is logged every 5 minutes, so 12 per hour (12 * 6 = 72) so mqtt will publish at batch size of 72/ every 6 hours
Loom_BatchSD batchSD(hypnos, 72);

void isrTrigger()
{
  hypnos.wakeup();
}


void setup() {
  ENABLE_SD_LOGGING;
  ENABLE_FUNC_SUMMARIES;

  // Wait 20 seconds for the serial console to open
  manager.beginSerial();

  // Set the LTE board to only powerup when a batch is ready to be sent
  lte.setBatchSD(batchSD);

  // Enable the hypnos rails
  hypnos.enable();

  // Read the MQTT creds file to supply the device with MQTT credentials
  mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));

  // Initialize all in-use modules
  manager.initialize();

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);

}

void loop() {

  // Measure and package the data
  manager.measure();
  manager.package();

  // Print the current JSON packet
  manager.display_data();

  // Log the data to the SD
  hypnos.logToSD();

  // Pass in the batchSD to the mqtt obj to check/ publish a batch of data if ready
  mqtt.publish(batchSD);

  // Set the interrupt duration for 5 minutes
  hypnos.setInterruptDuration(TimeSpan(0,0,5,0));

  // Reattach the interrupt
  hypnos.reattachRTCInterrupt();

  // Set the hypnos to sleep, but with power still being supplied to the 5v rail
  hypnos.sleep(false, true, false);
}

