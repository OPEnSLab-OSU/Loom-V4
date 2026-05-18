/**
 * MemPool + Hypnos sleep + SD logging demo.
 *
 * The board wakes every 20 seconds, builds a Manager JSON package, adds mock sensor data,
 * logs the packet to SD, then returns to sleep. SRAM remains powered during normal sleep,
 * so the Manager-owned MemPool remains resident across sleep/wake cycles.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include <Loom_Manager.h>
#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
Manager manager("Device", 1);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
uint32_t wakeCount = 0;
void isrTrigger() {
  hypnos.wakeup();
}
void addMockSensorData() {
  wakeCount++;
  manager.addData("MockSensor", "WakeCount", wakeCount);
  manager.addData("MockSensor", "Temperature_C", 21.5f + (wakeCount % 5) * 0.25f);
  manager.addData("MockSensor", "Humidity_RH", 45.0f + (wakeCount % 10));
  manager.addData("MockSensor", "Battery_V", 3.85f);
}
void setup() {
  manager.beginSerial();
  hypnos.enable();
  manager.initialize();
  hypnos.registerInterrupt(isrTrigger);
  manager.printPoolStats();
}
void loop() {
  hypnos.setInterruptDuration(TimeSpan(0, 0, 0, 20));
  manager.package();
  addMockSensorData();
  manager.display_data();
  hypnos.logToSD();
  manager.printPoolStats();
  hypnos.reattachRTCInterrupt();
  hypnos.sleep();
}