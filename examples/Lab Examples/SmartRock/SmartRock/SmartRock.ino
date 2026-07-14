/**
 * In lab use case example for the SmartRock project
 *
 * This project uses a Hypnos, ADS1115, MS5803, VCNL4010, and battery monitor.
 *
 * MANAGER MUST BE INCLUDED FIRST IN ALL CODE
 */

#include <Loom_Manager.h>

#include <Hardware/Loom_Hypnos/Loom_Hypnos.h>
#include <Sensors/I2C/Loom_ADS1115/Loom_ADS1115.h>
#include <Sensors/I2C/Loom_MS5803/Loom_MS5803.h>
#include <Sensors/Loom_Analog/Loom_Analog.h>

#include <Wire.h>
#include "Adafruit_VCNL4010.h"

Manager manager("Data", 1);
Loom_Analog analog(manager);
Loom_Hypnos hypnos(manager, HYPNOS_VERSION::V3_3, TIME_ZONE::PST);
Loom_ADS1115 ads(manager);
Loom_MS5803 ms(manager, 119);
Adafruit_VCNL4010 vcnl;

TimeSpan sleepInterval;
uint32_t wakeCycle = 0;

void isrTrigger();
bool takeData(float A0Offset, float A1Offset, float ecSlope, float ecIntercept,
              float turbiditySlope, float turbidityIntercept, bool troubleshootingMode);

void setup() {
  manager.beginSerial();

  hypnos.enable();
  manager.initialize();

  sleepInterval = hypnos.getConfigFromSD("SD_config.json");

  Serial.print(F("Sleep interval loaded from SD: "));
  Serial.print(sleepInterval.totalseconds());
  Serial.println(F(" seconds"));

  hypnos.registerInterrupt(isrTrigger);

  if (!vcnl.begin()) {
    Serial.println(F("VCNL4010 Not Found"));
  } else {
    Serial.println(F("VCNL4010 Initialized"));
  }
}

void loop() {
  const float A0Offset = -280.0f;
  const float A1Offset = 0.0f;
  const float ecSlope = 1.0f;
  const float ecIntercept = 0.0f;
  const float turbiditySlope = 0.0f;
  const float turbidityIntercept = 0.0f;
  const bool troubleshootingMode = true;

  wakeCycle++;

  if (troubleshootingMode) {
    Serial.println();
    Serial.print(F("Wake cycle: "));
    Serial.println(wakeCycle);
    Serial.println(F("Begin Taking Data"));
  }

  const bool sdLogged = takeData(
      A0Offset,
      A1Offset,
      ecSlope,
      ecIntercept,
      turbiditySlope,
      turbidityIntercept,
      troubleshootingMode);

  if (troubleshootingMode) {
    Serial.print(F("SD log result: "));
    Serial.println(sdLogged ? F("PASS") : F("FAIL"));
    Serial.print(F("Setting alarm after data collection for "));
    Serial.print(sleepInterval.totalseconds());
    Serial.println(F(" seconds"));
  }

  hypnos.setInterruptDuration(sleepInterval);
  hypnos.reattachRTCInterrupt();

  if (troubleshootingMode) {
    Serial.println(F("Going to Sleep"));
  }

  hypnos.sleep(false);
}

void isrTrigger() {
  hypnos.wakeup();
}

bool takeData(float A0Offset, float A1Offset, float ecSlope, float ecIntercept,
              float turbiditySlope, float turbidityIntercept, bool troubleshootingMode) {
  manager.measure();
  manager.package();

  if (troubleshootingMode) {
    Serial.println(F("Measured Data"));
  }

  const float A0 = ads.getAnalog(1) - A0Offset;
  const float A1 = ads.getAnalog(2) - A1Offset;
  const float conductivity = A1 != 0.0f
      ? ((A0 / A1) * ecSlope + ecIntercept)
      : 0.0f;
  const float proximity = static_cast<float>(vcnl.readProximity());
  const float turbidity = proximity * turbiditySlope + turbidityIntercept;

  if (troubleshootingMode) {
    Serial.println(F("EC and Turbidity Values Calculated"));
  }

  manager.addData("MS5803", "Pressure", ms.getPressure());
  manager.addData("MS5803", "Temperature", ms.getTemperature());
  manager.addData("vcnl4010", "Ambient Light", vcnl.readAmbient());
  manager.addData("vcnl4010", "Proximity", proximity);
  manager.addData("Analog Values", "A0_adjusted", A0);
  manager.addData("Analog Values", "A1_adjusted", A1);
  manager.addData("Analog Values", "Conductivity", conductivity);
  manager.addData("Analog Values", "Turbidity", turbidity);

  if (troubleshootingMode) {
    Serial.println(F("Data Added to Packet"));
  }

  manager.display_data();
  return hypnos.logToSD();
}
