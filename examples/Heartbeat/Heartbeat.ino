#include <Loom_Manager.h>

Manager manager("HeartbeatDevice", 1);

// Send a full data packet every 3 package calls; send heartbeat packets between them.
Heartbeat heartbeat(3);

void setup() {
  // Start the serial interface and wait for the user to open the serial monitor
  manager.beginSerial();

  // Attach heartbeat behavior before the first package call
  manager.useHeartbeat(&heartbeat);

  // Initialize the manager
  manager.initialize();
}

void loop() {
  // Package the data or heartbeat into JSON
  manager.package();

  // Print the JSON document to the Serial monitor
  manager.display_data();

  // Wait for 5 seconds
  manager.pause(5000);
}
