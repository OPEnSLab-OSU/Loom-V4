/**
 * This is an example use case for using the MPU gyroscope in conjunction with max in Access point mode
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Communication/Loom_Max.h>
#include <Sensors/I2C/Loom_MPU6050/Loom_MPU6050.h>


Manager manager("Device", 1);

// This puts the WiFI manager into AP mode
Loom_WIFI wifi(manager, true);
Loom_Max maxMsp(manager, wifi, CommunicationMode::AP);
Loom_MPU6050 mpu(manager);


void setup() {

  manager.beginSerial();
  manager.initialize();
}

void loop() {
  manager.measure();
  manager.package();
  manager.display_data();

  // Send and Recieve data from Max
  maxMsp.publish();
  //maxMsp.subscribe();
  delay(50);
}