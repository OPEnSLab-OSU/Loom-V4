/**
 * This is an example use case for using the MPU gyroscope in conjunction with max
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Publishing/Loom_Max.h>
#include <Sensors/I2C/Loom_MPU6050/Loom_MPU6050.h>


Manager manager("Device", 1);

Loom_WIFI wifi(manager, SECRET_SSID, SECRET_PASS);
Loom_Max maxMsp(manager, wifi);
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