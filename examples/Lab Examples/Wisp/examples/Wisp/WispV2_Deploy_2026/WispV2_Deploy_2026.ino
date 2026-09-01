
#include <Loom_Manager.h>

// BEGIN LOOM_BETA_DIAGNOSTICS
// Temporary soak-test instrumentation. Set to 0 for a clean field build. For the final
// canonical example, remove this block and every line tagged LOOM_BETA_DIAGNOSTIC.
#ifndef LOOM_WISP_BETA_DIAGNOSTICS
#define LOOM_WISP_BETA_DIAGNOSTICS 1
#endif

#if LOOM_WISP_BETA_DIAGNOSTICS
#include <Diagnostics/Loom_MemoryDiagnostics.h>
#endif
// END LOOM_BETA_DIAGNOSTICS

#include <Hardware/Loom_Multiplexer/Loom_Multiplexer.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Internet/Connectivity/Loom_LTE/Loom_LTE.h>
#include <Logger.h>
#include <Internet/Logging/Loom_MongoDB/Loom_MongoDB.h>
#include <Adafruit_SleepyDog.h>

constexpr int ACTIVE_WATCHDOG_MS = 16000;

void enableActiveWatchdog()
{
  Watchdog.enable(ACTIVE_WATCHDOG_MS);
  Watchdog.reset();
}

Manager manager("Deploy_Test_", 8);

Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST, true);

Loom_LTE lte(manager, "hologram", "", "");
Loom_MongoDB mqtt(manager, lte);
// Twelve five-minute records per hour produce one 72-record publish every six hours.
Loom_BatchSD batchSD(hypnos, 72);

// BEGIN LOOM_BETA_DIAGNOSTICS
#if LOOM_WISP_BETA_DIAGNOSTICS
Loom_MemoryDiagnostics memoryDiagnostics;
#define WISP_DIAGNOSTIC_BEGIN_CYCLE() memoryDiagnostics.beginCycle()
#define WISP_DIAGNOSTIC_CHECKPOINT(phaseLabel)                                      \
  memoryDiagnostics.checkpoint(F(phaseLabel), manager.getDocument(),                \
                               batchSD.getCurrentBatch())
#define WISP_DIAGNOSTIC_ENABLE_MUX_SCAN() do { mux.setDebug(true); mux.setScanDebug(true); } while (false)
#define WISP_DIAGNOSTIC_ENABLE_SD_TRACE() hypnos.getSDManager()->setWriteDebug(true)
#else
#define WISP_DIAGNOSTIC_BEGIN_CYCLE() do { } while (false)
#define WISP_DIAGNOSTIC_CHECKPOINT(phaseLabel) do { } while (false)
#define WISP_DIAGNOSTIC_ENABLE_MUX_SCAN() do { } while (false)
#define WISP_DIAGNOSTIC_ENABLE_SD_TRACE() do { } while (false)
#endif
// END LOOM_BETA_DIAGNOSTICS

// Reads the battery voltage
Loom_Analog analog(manager);

Loom_Multiplexer mux(manager, {0x74, 0x15, 0x6B, 0x44});

void isrTrigger()
{
  hypnos.wakeup();
}

void setup() {

  // Preserve the canonical timestamped debug log in /debug/output_N.log.
  ENABLE_SD_LOGGING;
  
  // DISABLE FUNC SUMMARIES FOR FIELD DEPLOYMENT!
  // ENABLE_FUNC_SUMMARIES; 

  // Start the serial interface
  manager.beginSerial();
  WISP_DIAGNOSTIC_ENABLE_SD_TRACE(); // LOOM_BETA_DIAGNOSTIC
  WISP_DIAGNOSTIC_ENABLE_MUX_SCAN(); // LOOM_BETA_DIAGNOSTIC
  WISP_DIAGNOSTIC_CHECKPOINT("post_global_ctor"); // LOOM_BETA_DIAGNOSTIC

  // Set the LTE board to only powerup when a batch is ready to be sent
  lte.setBatchSD(batchSD);

  // Both power rails should be on when awake
  hypnos.setWakeConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

  // Only the 5V rail should be on during sleep
  hypnos.setSleepConfiguration(POWERRAIL_CONFIG::PR_3V_ON_5V_ON);

  // Non-interactive fallback if the RTC backup supply was lost in the field.
  hypnos.setCompileTime(__DATE__, __TIME__);

  // Enable the hypnos rails
  hypnos.enable();
  
  // Synchronize time using LTE.
  hypnos.setNetworkInterface(&lte);

  // Read the MQTT creds file to supply the device with MQTT credentials
  WISP_DIAGNOSTIC_CHECKPOINT("pre_mqtt_config"); // LOOM_BETA_DIAGNOSTIC
  mqtt.loadConfigFromJSON(hypnos.readFile("mqtt_creds.json"));
  WISP_DIAGNOSTIC_CHECKPOINT("post_mqtt_config"); // LOOM_BETA_DIAGNOSTIC

  // Initialize the manager (LTE initialization takes ~15 seconds, so do this BEFORE starting the Watchdog)
  WISP_DIAGNOSTIC_CHECKPOINT("pre_initialize"); // LOOM_BETA_DIAGNOSTIC
  manager.initialize();
  WISP_DIAGNOSTIC_CHECKPOINT("post_initialize"); // LOOM_BETA_DIAGNOSTIC

  // Register the ISR and attach to the interrupt
  hypnos.registerInterrupt(isrTrigger);

  WISP_DIAGNOSTIC_CHECKPOINT("pre_initial_time_sync"); // LOOM_BETA_DIAGNOSTIC
  hypnos.networkTimeUpdate();
  WISP_DIAGNOSTIC_CHECKPOINT("post_initial_time_sync"); // LOOM_BETA_DIAGNOSTIC

  WISP_DIAGNOSTIC_CHECKPOINT("setup_done"); // LOOM_BETA_DIAGNOSTIC
}

void loop() {

  enableActiveWatchdog();
  WISP_DIAGNOSTIC_BEGIN_CYCLE(); // LOOM_BETA_DIAGNOSTIC
  WISP_DIAGNOSTIC_CHECKPOINT("loop_start"); // LOOM_BETA_DIAGNOSTIC

  // Measure the data from the sensors
  WISP_DIAGNOSTIC_CHECKPOINT("pre_measure"); // LOOM_BETA_DIAGNOSTIC
  manager.measure();
  WISP_DIAGNOSTIC_CHECKPOINT("post_measure"); // LOOM_BETA_DIAGNOSTIC

  // Pet the dog again just in case measure took a few seconds
  Watchdog.reset(); 

  // Package the data into JSON
  manager.package();
  Watchdog.reset();
  WISP_DIAGNOSTIC_CHECKPOINT("post_package"); // LOOM_BETA_DIAGNOSTIC

  // Print the JSON document to the Serial monitor
  WISP_DIAGNOSTIC_CHECKPOINT("pre_display"); // LOOM_BETA_DIAGNOSTIC
  manager.display_data();
  Watchdog.reset();
  WISP_DIAGNOSTIC_CHECKPOINT("post_display"); // LOOM_BETA_DIAGNOSTIC

  // Log the data to the SD
  WISP_DIAGNOSTIC_CHECKPOINT("pre_sd"); // LOOM_BETA_DIAGNOSTIC
  hypnos.logToSD();
  Watchdog.reset();
  WISP_DIAGNOSTIC_CHECKPOINT("post_sd"); // LOOM_BETA_DIAGNOSTIC
  
  // Pass in the batchSD to the mqtt obj to check/ publish a batch of data if ready
  WISP_DIAGNOSTIC_CHECKPOINT("pre_mqtt"); // LOOM_BETA_DIAGNOSTIC
  const bool networkWindow = batchSD.getCurrentBatch() >= batchSD.getBatchSize();
  if (networkWindow) {
    Watchdog.disable();
  }
  else {
    Watchdog.reset();
  }
  mqtt.publish(batchSD);
  if (networkWindow) {
    enableActiveWatchdog();
  }
  else {
    Watchdog.reset();
  }
  WISP_DIAGNOSTIC_CHECKPOINT("post_mqtt"); // LOOM_BETA_DIAGNOSTIC
 
  // Set the interrupt duration for 5 minutes
  WISP_DIAGNOSTIC_CHECKPOINT("pre_rtc"); // LOOM_BETA_DIAGNOSTIC
  Watchdog.reset();
  hypnos.setInterruptDuration(TimeSpan(0, 0, 5, 0));
  Watchdog.reset();

  // Reattach the interrupt
  hypnos.reattachRTCInterrupt();
  Watchdog.reset();
  WISP_DIAGNOSTIC_CHECKPOINT("post_rtc"); // LOOM_BETA_DIAGNOSTIC
 
  // Sync time (network updates can also block for several seconds)
  WISP_DIAGNOSTIC_CHECKPOINT("pre_time_sync"); // LOOM_BETA_DIAGNOSTIC
  Watchdog.disable();
  hypnos.networkTimeUpdate();
  enableActiveWatchdog();
  WISP_DIAGNOSTIC_CHECKPOINT("post_time_sync"); // LOOM_BETA_DIAGNOSTIC
  
  // Set the hypnos to sleep
  WISP_DIAGNOSTIC_CHECKPOINT("pre_sleep"); // LOOM_BETA_DIAGNOSTIC
  // The SAMD21 watchdog continues in standby, so disable it for the five-minute RTC sleep.
  Watchdog.disable();
  hypnos.sleep(false);
  WISP_DIAGNOSTIC_CHECKPOINT("post_wake"); // LOOM_BETA_DIAGNOSTIC

}
