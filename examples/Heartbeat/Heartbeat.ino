#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>

Manager manager("HeartbeatDevice", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);

// Send a full data packet every 3 package calls; send heartbeat packets between them.
Heartbeat heartbeat(3);

void isrTrigger(){
  hypnos.wakeup();
}

void setup() {
  manager.beginSerial();

  // register heartbeat
  manager.useHeartbeat(&heartbeat);

  hypnos.enable();

  manager.initialize();

  hypnos.registerInterrupt(isrTrigger);
}

void loop() {
  hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 10));

  manager.measure();

  manager.package();

  manager.display_data();

  // log to SD
  hypnos.logToSD();

  hypnos.reattachRTCInterrupt();
  
  hypnos.sleep();
}
