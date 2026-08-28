
#include <Loom_Manager.h>
#include <Diagnostics/Loom_MemoryDiagnostics.h>
#include <Hardware/Loom_Multiplexer/Loom_Multiplexer.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Logger.h>
#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Adafruit_SleepyDog.h> 

Manager manager("Deploy_Test_", 8);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

Loom_LTE lte(manager, "hologram", "", "");
Loom_MongoDB mqtt(manager, lte);
//A batch is logged every 5 minutes, so 12 per hour (12 * 6 = 72) so mqtt will publish at batch size of 72/ every 6 hours
Loom_BatchSD batchSD(hypnos, 72);
Loom_MemoryDiagnostics memoryDiagnostics;

// Reads the battery voltage
Loom_Analog analog(manager);

Loom_Multiplexer mux(manager , {0x74, 0x15, 0x6B, 0x44});

void isrTrigger()
{
  hypnos.wakeup();
}

void setup() {

  ENABLE_SD_LOGGING;
  
  // DISABLE FUNC SUMMARIES FOR FIELD DEPLOYMENT!
  // ENABLE_FUNC_SUMMARIES; 

  // Start the serial interface
  manager.beginSerial();
  memoryDiagnostics.checkpoint(F("post_global_ctor"), manager.getDocument(),
                               batchSD.getCurrentBatch());

  // Set the LTE board to only powerup when a batch is ready to be sent
  lte.setBatchSD(batchSD);

  // Both power rails should be on when awake
  hypnos.setWakeConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

  // Only the 5V rail should be on during sleep
  hypnos.setSleepConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

  // Enable the hypnos rails
  hypnos.enable();
  
  //Time Sync Using LTE 
  hypnos.setNetworkInterface(&lte);

  // Read the MQTT creds file to supply the device with MQTT credentials
  memoryDiagnostics.checkpoint(F("pre_mqtt_config"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
  memoryDiagnostics.checkpoint(F("post_mqtt_config"), manager.getDocument(),
                               batchSD.getCurrentBatch());

  // Initialize the manager (LTE initialization takes ~15 seconds, so do this BEFORE starting the Watchdog)
  memoryDiagnostics.checkpoint(F("pre_initialize"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  manager.initialize();
  memoryDiagnostics.checkpoint(F("post_initialize"), manager.getDocument(),
                               batchSD.getCurrentBatch());

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);

  memoryDiagnostics.checkpoint(F("pre_initial_time_sync"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  hypnos.networkTimeUpdate();
  memoryDiagnostics.checkpoint(F("post_initial_time_sync"), manager.getDocument(),
                               batchSD.getCurrentBatch());

  memoryDiagnostics.checkpoint(F("setup_done"), manager.getDocument(),
                               batchSD.getCurrentBatch());
}

void loop() {

  memoryDiagnostics.beginCycle();
  memoryDiagnostics.checkpoint(F("loop_start"), manager.getDocument(),
                               batchSD.getCurrentBatch());

  Watchdog.enable(16000); 
  Watchdog.reset();

  // Measure the data from the sensors
  memoryDiagnostics.checkpoint(F("pre_measure"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  manager.measure();
  memoryDiagnostics.checkpoint(F("post_measure"), manager.getDocument(),
                               batchSD.getCurrentBatch());

  // Pet the dog again just in case measure took a few seconds
  Watchdog.reset(); 

  // Package the data into JSON
  manager.package();
  memoryDiagnostics.checkpoint(F("post_package"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  memoryDiagnostics.addToPacket(manager, batchSD.getCurrentBatch());

  // Print the JSON document to the Serial monitor
  memoryDiagnostics.checkpoint(F("pre_display"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  manager.display_data();
  memoryDiagnostics.checkpoint(F("post_display"), manager.getDocument(),
                               batchSD.getCurrentBatch());

  // Log the data to the SD
  memoryDiagnostics.checkpoint(F("pre_sd"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  hypnos.logToSD();
  memoryDiagnostics.checkpoint(F("post_sd"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  
  // Disable watchdog
  Watchdog.disable(); 

  // Pass in the batchSD to the mqtt obj to check/ publish a batch of data if ready
  memoryDiagnostics.checkpoint(F("pre_mqtt"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  mqtt.publish(batchSD);
  memoryDiagnostics.checkpoint(F("post_mqtt"), manager.getDocument(),
                               batchSD.getCurrentBatch());
 
  // Set the interrupt duration for 5 minutes
  memoryDiagnostics.checkpoint(F("pre_rtc"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  hypnos.setInterruptDuration(TimeSpan(0,0,5,0));

  // Reattach the interrupt
  hypnos.reattachRTCInterrupt();
  memoryDiagnostics.checkpoint(F("post_rtc"), manager.getDocument(),
                               batchSD.getCurrentBatch());
 
  // Sync time (network updates can also block for several seconds)
  memoryDiagnostics.checkpoint(F("pre_time_sync"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  hypnos.networkTimeUpdate();
  memoryDiagnostics.checkpoint(F("post_time_sync"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  
  // Set the hypnos to sleep
  memoryDiagnostics.checkpoint(F("pre_sleep"), manager.getDocument(),
                               batchSD.getCurrentBatch());
  hypnos.sleep(false);
  memoryDiagnostics.checkpoint(F("post_wake"), manager.getDocument(),
                               batchSD.getCurrentBatch());

}
