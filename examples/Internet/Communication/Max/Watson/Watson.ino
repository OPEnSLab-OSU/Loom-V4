/**
 * This is an example use case for using the MPU gyroscope in conjunction with max
 * 
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */
#include "arduino_secrets.h"

#include <Loom_Manager.h>

#include <Internet/Connectivity/Loom_Wifi/Loom_Wifi.h>
#include <Internet/Communication/Loom_Max.h>
#include <Sensors/I2C/Loom_MPU6050/Loom_MPU6050.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>
#include <Sensors/Loom_Digital/Loom_Digital.h>
#include <Hardware/Actuators/Loom_Neopixel/Loom_Neopixel.h>

Manager manager("Device", 1);
//Loom_WIFI wifi(manager, CommunicationMode::AP); // For AP
Loom_WIFI wifi(manager, CommunicationMode::CLIENT, SECRET_SSID, SECRET_PASS); // For Client
Loom_Max maxMsp(manager, wifi new Loom_Neopixel());

Loom_MPU6050 mpu(manager);
// Read the battery voltage and A0 and A1
Loom_Analog analog(manager, A0, A1);
// Reads the button on pin 10
Loom_Digital digital(manager, 10);

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
  maxMsp.subscribe();
  manager.pause(50);
}