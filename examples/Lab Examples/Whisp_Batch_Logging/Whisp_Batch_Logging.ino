#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
// #include <Sensors/I2C/Loom_MultiGasSensor/Loom_MultiGasSensor.h>

#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/I2C/Loom_SEN55/Loom_SEN55.h>
#include <Sensors/I2C/Loom_SHT31/Loom_SHT31.h>
#include <Sensors/I2C/Loom_T6793/Loom_T6793.h>
#include <Sensors/I2C/Loom_MultiGasSensor/Loom_MultiGasSensor.h>


//#include <Sensors/Analog/ACS712/Loom_ACS712.h>

#include <Logger.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>

Manager manager("Whisp_brd_v0p4_", 1); //change

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

Loom_Analog analog(manager);

//Main Air Quality, Temperature, Humidity Sensing, CO2, Gravity
Loom_SEN55 SEN55(manager);
Loom_SHT31 sht(manager);

Loom_T6793 T6793(manager);
Loom_MultiGasSensor gasSensor(manager, 0x77, false);

uint32_t deviceStatus;

//Connectivity
Loom_LTE lte(manager, "hologram", "", "");
Loom_MongoDB mqtt(manager, lte);
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

  // Both power rails should be on when awake
  hypnos.setWakeConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

  // Only the 5V rail should be on during sleep
  hypnos.setSleepConfiguration(POWERRAIL_CONFIG::PR_3V_OFF_5V_ON);

  // Enable the hypnos rails
  hypnos.enable();

  //Time Sync Using LTE 
  hypnos.setNetworkInterface(&lte);

  // Read the MQTT creds file to supply the device with MQTT credentials
  mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));

  // Initialize all in-use modules
  manager.initialize();

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);

  hypnos.networkTimeUpdate();

  // SEN55.logDeviceStatus();

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
  //mqtt.publish();

  // Set the interrupt duration for 5 minutes
  hypnos.setInterruptDuration(TimeSpan(0,0,5,0));

  // Reattach the interrupt
  hypnos.reattachRTCInterrupt();

  SEN55.logDeviceStatus();
  
  // Set the hypnos to sleep, but with power still being supplied to the 5v rail (wait for serial when testing from a computer)
  hypnos.sleep(false);


  hypnos.networkTimeUpdate();
  //manager.pause(2000);
}